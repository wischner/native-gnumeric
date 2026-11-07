//
// Text functions. Positions count characters, not bytes, so the bodies
// work in UTF-32.
//
// Derived from Gnumeric plugins/fn-string/functions.c,
// Copyright (C) 1998-2002 Miguel de Icaza, Jody Goldberg, Morten Welinder.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "func/collect.h"
#include "numfmt/format.h"
#include "numfmt/match.h"
#include "util/text.h"
#include "util/utf8.h"

namespace func {
namespace {

using util::to_utf32;
using util::to_utf8;

// Return a character count argument; negative counts are an error.
std::optional<std::size_t> count_arg(args_t a, std::size_t index,
                                     double fallback) {
    const double count = std::floor(number_or(a, index, fallback));
    if (count < 0)
        return std::nullopt;
    return static_cast<std::size_t>(std::min(count, 1e9));
}

value left_of(call_context &, args_t a) {
    const auto count = count_arg(a, 1, 1);
    if (!count)
        return value(error_code::value);
    const std::u32string text = to_utf32(a[0].string());
    return value(to_utf8(text.substr(0, *count)));
}

value right_of(call_context &, args_t a) {
    const auto count = count_arg(a, 1, 1);
    if (!count)
        return value(error_code::value);
    const std::u32string text = to_utf32(a[0].string());
    const std::size_t keep = std::min(*count, text.size());
    return value(to_utf8(text.substr(text.size() - keep)));
}

value mid_of(call_context &, args_t a) {
    const double start = std::floor(a[1].number());
    const double count = std::floor(a[2].number());
    if (start < 1 || count < 0)
        return value(error_code::value);
    const std::u32string text = to_utf32(a[0].string());
    if (start - 1 >= static_cast<double>(text.size()))
        return value("");
    return value(to_utf8(text.substr(static_cast<std::size_t>(start) - 1,
                                     static_cast<std::size_t>(count))));
}

value replace_of(call_context &, args_t a) {
    const double start = std::floor(a[1].number());
    const double count = std::floor(a[2].number());
    if (start < 1 || count < 0)
        return value(error_code::value);
    const std::u32string text = to_utf32(a[0].string());
    const std::size_t from = std::min(static_cast<std::size_t>(start) - 1,
                                      text.size());
    const std::size_t to =
        std::min(from + static_cast<std::size_t>(count), text.size());
    return value(to_utf8(text.substr(0, from)) + a[3].string() +
                 to_utf8(text.substr(to)));
}

value substitute_of(call_context &, args_t a) {
    const std::string &text = a[0].string();
    const std::string &old_text = a[1].string();
    const std::string &new_text = a[2].string();
    const double which = number_or(a, 3, 0);
    if (a.size() > 3 && !a[3].is_empty() && which < 1)
        return value(error_code::value);
    if (old_text.empty())
        return a[0];
    std::string out;
    std::size_t at = 0;
    int seen = 0;
    while (true) {
        const std::size_t found = text.find(old_text, at);
        if (found == std::string::npos)
            break;
        ++seen;
        out.append(text, at, found - at);
        at = found + old_text.size();
        if (which == 0 || seen == static_cast<int>(which))
            out += new_text;
        else
            out += old_text;
    }
    out.append(text, at, std::string::npos);
    return value(out);
}

// FIND and SEARCH: one-based position of a needle.
template <bool ignore_case, bool wildcards>
value locate(call_context &, args_t a) {
    const double start = number_or(a, 2, 1);
    if (start < 1)
        return value(error_code::value);
    std::u32string needle =
        to_utf32(ignore_case ? util::to_lower(a[0].string()) : a[0].string());
    std::u32string haystack =
        to_utf32(ignore_case ? util::to_lower(a[1].string()) : a[1].string());
    const auto from = static_cast<std::size_t>(start) - 1;
    if (from > haystack.size())
        return value(error_code::value);
    if constexpr (wildcards) {
        const std::string pattern = a[0].string() + "*";
        for (std::size_t i = from; i <= haystack.size(); ++i)
            if (wildcard_match(pattern, to_utf8(haystack.substr(i))))
                return value(static_cast<double>(i + 1));
        return value(error_code::value);
    } else {
        const std::size_t found = haystack.find(needle, from);
        if (found == std::u32string::npos)
            return value(error_code::value);
        return value(static_cast<double>(found + 1));
    }
}

value trim_of(call_context &, args_t a) {
    std::string out;
    for (const char c : a[0].string()) {
        if (c == ' ' && (out.empty() || out.back() == ' '))
            continue;
        out.push_back(c);
    }
    if (!out.empty() && out.back() == ' ')
        out.pop_back();
    return value(out);
}

value clean_of(call_context &, args_t a) {
    std::string out;
    for (const char c : a[0].string())
        if (static_cast<unsigned char>(c) >= 32)
            out.push_back(c);
    return value(out);
}

value proper_of(call_context &, args_t a) {
    std::u32string text = to_utf32(a[0].string());
    bool in_word = false;
    for (char32_t &c : text) {
        const std::string one = to_utf8(std::u32string_view(&c, 1));
        const bool letter = util::is_word_character(c);
        c = to_utf32(in_word ? util::to_lower(one) : util::to_upper(one))[0];
        in_word = letter;
    }
    return value(to_utf8(text));
}

value repeat_of(call_context &, args_t a) {
    const double count = std::floor(a[1].number());
    if (count < 0 || count * a[0].string().size() > 32767 * 4)
        return value(error_code::value);
    std::string out;
    for (int i = 0; i < count; ++i)
        out += a[0].string();
    return value(out);
}

value char_of(call_context &, args_t a) {
    const double code = std::floor(a[0].number());
    if (code < 1 || code >= 0x110000)
        return value(error_code::value);
    std::string out;
    util::append_utf8(out, static_cast<char32_t>(code));
    return value(out);
}

value code_of(call_context &, args_t a) {
    const std::u32string text = to_utf32(a[0].string());
    if (text.empty())
        return value(error_code::value);
    return value(static_cast<double>(text[0]));
}

value text_of(call_context &, args_t a) {
    const auto format = numfmt::number_format::get(a[1].string());
    if (a[0].is_string()) {
        const auto number = a[0].to_number();
        if (!number)
            return value(format->render_text(a[0].string()).text);
        return value(format->render(*number).text);
    }
    if (a[0].is_error())
        return a[0];
    const auto number = a[0].to_number();
    if (!number)
        return value(error_code::value);
    return value(format->render(*number).text);
}

value value_of(call_context &, args_t a) {
    if (a[0].is_numeric())
        return a[0];
    if (a[0].is_error())
        return a[0];
    const auto number = a[0].to_number();
    return number ? value(*number) : value(error_code::value);
}

value number_value(call_context &, args_t a) {
    std::string text = a[0].string();
    const std::string decimal =
        a.size() > 1 && !a[1].is_empty() ? a[1].string() : ".";
    const std::string group =
        a.size() > 2 && !a[2].is_empty() ? a[2].string() : ",";
    std::string cleaned;
    for (const char c : text) {
        if (c == ' ' || (!group.empty() && c == group[0]))
            continue;
        cleaned.push_back(!decimal.empty() && c == decimal[0] ? '.' : c);
    }
    const auto match = numfmt::match_number(cleaned);
    return match ? value(match->value) : value(error_code::value);
}

// Round to decimals and insert separators for FIXED and DOLLAR.
std::string with_commas(double x, int decimals, bool group) {
    const double p = std::pow(10.0, decimals);
    const double rounded = std::round(x * p) / p;
    char buffer[400];
    std::snprintf(buffer, sizeof buffer, "%.*f", std::max(decimals, 0),
                  std::fabs(rounded));
    std::string body = buffer;
    if (group) {
        const auto point = body.find('.');
        auto at = point == std::string::npos ? body.size() : point;
        while (at > 3) {
            at -= 3;
            body.insert(at, ",");
        }
    }
    return body;
}

value fixed(call_context &, args_t a) {
    const double x = a[0].number();
    const int decimals = static_cast<int>(number_or(a, 1, 2));
    const bool no_commas = flag_or(a, 2, false);
    const std::string body = with_commas(x, decimals, !no_commas);
    return value(x < 0 ? "-" + body : body);
}

value dollar(call_context &, args_t a) {
    const double x = a[0].number();
    const int decimals = static_cast<int>(number_or(a, 1, 2));
    const std::string body = "$" + with_commas(x, decimals, true);
    return value(x < 0 ? "(" + body + ")" : body);
}

value concat(call_context &context, nodes_t nodes) {
    std::string out;
    auto failed = for_each_value(
        context, nodes, [&](const value &item) -> std::optional<value> {
            if (item.is_error())
                return item;
            out += item.to_text();
            return std::nullopt;
        });
    return failed ? *failed : value(out);
}

value text_join(call_context &context, nodes_t nodes) {
    if (nodes.size() < 2)
        return value(error_code::value);
    const value separator =
        nodes[0]->eval(context.pos, expr::eval_mode::scalar);
    if (separator.is_error())
        return separator;
    const value skip = nodes[1]->eval(context.pos, expr::eval_mode::scalar);
    const bool ignore_empty = skip.to_bool().value_or(true);
    const std::string glue = separator.to_text();
    std::string out;
    bool first = true;
    auto failed = for_each_value(
        context, nodes.subspan(2),
        [&](const value &item) -> std::optional<value> {
            if (item.is_error())
                return item;
            const std::string text = item.to_text();
            if (ignore_empty && text.empty())
                return std::nullopt;
            if (!first)
                out += glue;
            first = false;
            out += text;
            return std::nullopt;
        });
    return failed ? *failed : value(out);
}

const function_row rows[] = {
    {"LEFT", "s|f", left_of, "the first characters"},
    {"RIGHT", "s|f", right_of, "the last characters"},
    {"MID", "sff", mid_of, "characters from a position"},
    {"LEN", "s",
     [](call_context &, args_t a) -> value {
         return value(static_cast<double>(util::char_count(a[0].string())));
     },
     "length in characters"},
    {"LENB", "s",
     [](call_context &, args_t a) -> value {
         return value(static_cast<double>(a[0].string().size()));
     },
     "length in bytes"},
    {"LOWER", "s",
     [](call_context &, args_t a) -> value {
         return value(util::to_lower(a[0].string()));
     },
     "lower case"},
    {"UPPER", "s",
     [](call_context &, args_t a) -> value {
         return value(util::to_upper(a[0].string()));
     },
     "upper case"},
    {"PROPER", "s", proper_of, "capitalized words"},
    {"TRIM", "s", trim_of, "remove extra spaces"},
    {"CLEAN", "s", clean_of, "remove control characters"},
    {"REPT", "sf", repeat_of, "repeated text"},
    {"REPLACE", "sffs", replace_of, "replace characters by position"},
    {"SUBSTITUTE", "sss|f", substitute_of, "replace text by content"},
    {"FIND", "ss|f", locate<false, false>, "position of text"},
    {"SEARCH", "ss|f", locate<true, true>,
     "position of a pattern, any case"},
    {"EXACT", "ss",
     [](call_context &, args_t a) -> value {
         return value(a[0].string() == a[1].string());
     },
     "equality with case"},
    {"CHAR", "f", char_of, "the character of a code"},
    {"UNICHAR", "f", char_of, "the character of a code point"},
    {"CODE", "s", code_of, "the code of a character"},
    {"UNICODE", "s", code_of, "the code point of a character"},
    {"T", "S",
     [](call_context &, args_t a) -> value {
         return a[0].is_string() ? a[0] : value("");
     },
     "text, or nothing for other values"},
    {"TEXT", "Ss", text_of, "a value through a format"},
    {"VALUE", "S", value_of, "the number a text spells"},
    {"NUMBERVALUE", "s|ss", number_value,
     "a number with given separators"},
    {"FIXED", "f|fb", fixed, "a number as text"},
    {"DOLLAR", "f|f", dollar, "a number as currency text"},
};

const variadic_row variadic[] = {
    {"CONCATENATE", concat, "join text"},
    {"CONCAT", concat, "join text and ranges"},
    {"TEXTJOIN", text_join, "join text with a delimiter"},
};

} // namespace

void register_string(expr::function_table &table) {
    table.add_rows("String", rows);
    add_variadic(table, "String", variadic);
}

} // namespace func
