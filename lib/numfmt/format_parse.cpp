//
// Parses the format language into tokens. The scanner is one loop over
// the characters because the language is context-sensitive only in
// two places, both resolved afterwards: "m" means minutes next to an
// hour or second, and a comma scales when no digit follows it.
//
// Derived from goffice go-format.c,
// Copyright (C) 2005-2015 Morten Welinder, Jody Goldberg and contributors.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include <array>
#include <charconv>
#include <cstdlib>

#include "format_section.h"
#include "util/text.h"

namespace numfmt {
namespace {

// One color name a section may open with.
struct color_name {
    std::string_view name;
    util::color value;
};

constexpr std::array<color_name, 8> color_names{{
    {"black", {0, 0, 0}},
    {"blue", {0, 0, 255}},
    {"cyan", {0, 255, 255}},
    {"green", {0, 255, 0}},
    {"magenta", {255, 0, 255}},
    {"red", {255, 0, 0}},
    {"white", {255, 255, 255}},
    {"yellow", {255, 255, 0}},
}};

// Excel's first eight indexed colors; others fall to black.
constexpr std::array<util::color, 8> indexed_colors{{
    {0, 0, 0},
    {255, 255, 255},
    {255, 0, 0},
    {0, 255, 0},
    {0, 0, 255},
    {255, 255, 0},
    {255, 0, 255},
    {0, 255, 255},
}};

// Scans one format string.
class scanner {
public:
    explicit scanner(std::string_view text) : text_(text) {}

    // Parse every section.
    std::vector<format_section> run() {
        std::vector<format_section> sections(1);
        current_ = &sections.back();
        while (at_ < text_.size()) {
            const char c = text_[at_];
            if (c == ';') {
                ++at_;
                finish(*current_);
                sections.emplace_back();
                current_ = &sections.back();
                in_decimals_ = in_exponent_ = in_denominator_ = false;
                continue;
            }
            step(c);
        }
        finish(*current_);
        return sections;
    }

private:
    // Append literal text, joining neighboring literals.
    void literal(std::string_view text) {
        auto &tokens = current_->tokens;
        if (!tokens.empty() && tokens.back().kind == token_kind::literal)
            tokens.back().text += text;
        else
            tokens.push_back({token_kind::literal, std::string(text), 0});
    }

    // Append a token with no payload.
    void token(token_kind kind, int size = 0) {
        current_->tokens.push_back({kind, {}, size});
    }

    // Return how many times a letter repeats, any case.
    std::size_t run_length(char lower) const {
        std::size_t length = 0;
        while (at_ + length < text_.size() &&
               (text_[at_ + length] | 0x20) == lower)
            ++length;
        return length;
    }

    // Report whether text at the cursor matches, ignoring case.
    bool looking_at(std::string_view word) const {
        return at_ + word.size() <= text_.size() &&
               util::equal_nocase(text_.substr(at_, word.size()), word);
    }

    // Handle one bracketed directive.
    void bracket() {
        const auto close = text_.find(']', at_);
        if (close == std::string_view::npos) {
            literal(text_.substr(at_));
            at_ = text_.size();
            return;
        }
        const std::string_view body = text_.substr(at_ + 1, close - at_ - 1);
        at_ = close + 1;
        if (body.empty())
            return;
        const std::string lowered = util::to_lower(body);
        if (lowered == "h" || lowered == "hh") {
            token(token_kind::elapsed_hours,
                  static_cast<int>(lowered.size()));
            current_->temporal = current_->has_time = true;
            return;
        }
        if (lowered == "m" || lowered == "mm") {
            token(token_kind::elapsed_minutes,
                  static_cast<int>(lowered.size()));
            current_->temporal = current_->has_time = true;
            return;
        }
        if (lowered == "s" || lowered == "ss") {
            token(token_kind::elapsed_seconds,
                  static_cast<int>(lowered.size()));
            current_->temporal = current_->has_time = true;
            return;
        }
        if (body[0] == '$') {
            const auto dash = body.find('-');
            const std::string_view symbol =
                body.substr(1, dash == std::string_view::npos
                                   ? std::string_view::npos
                                   : dash - 1);
            literal(symbol);
            if (!symbol.empty())
                current_->has_currency = true;
            return;
        }
        if (body[0] == '<' || body[0] == '>' || body[0] == '=') {
            condition(body);
            return;
        }
        for (const color_name &entry : color_names)
            if (lowered == entry.name) {
                current_->color = entry.value;
                return;
            }
        if (lowered.rfind("color", 0) == 0) {
            const std::size_t index = static_cast<std::size_t>(
                std::atoi(lowered.c_str() + 5));
            current_->color = index >= 1 && index <= indexed_colors.size()
                                  ? indexed_colors[index - 1]
                                  : util::color{};
        }
        // Anything else is a locale or an extension with no display.
    }

    // Parse a section condition such as >=100.
    void condition(std::string_view body) {
        section_condition result;
        std::size_t length = 1;
        using op = section_condition::op;
        if (body.substr(0, 2) == "<=") {
            result.comparison = op::le;
            length = 2;
        } else if (body.substr(0, 2) == ">=") {
            result.comparison = op::ge;
            length = 2;
        } else if (body.substr(0, 2) == "<>") {
            result.comparison = op::ne;
            length = 2;
        } else if (body[0] == '<') {
            result.comparison = op::lt;
        } else if (body[0] == '>') {
            result.comparison = op::gt;
        } else {
            result.comparison = op::eq;
        }
        const std::string number(body.substr(length));
        result.limit = std::strtod(number.c_str(), nullptr);
        current_->condition = result;
    }

    // Handle a digit placeholder.
    void digit(char c) {
        const token_kind kind = c == '0'   ? token_kind::digit_zero
                                : c == '#' ? token_kind::digit_hash
                                           : token_kind::digit_blank;
        token(kind);
        current_->has_digits = true;
        if (in_exponent_)
            ++current_->exponent_digits;
        else if (in_denominator_)
            ++current_->denominator_digits;
        else if (in_decimals_)
            ++current_->decimal_digits;
        else
            ++current_->integer_digits;
        ++at_;
    }

    // Handle one date or time letter run.
    bool temporal_letter(char lowered) {
        const auto add = [&](token_kind kind, std::size_t length) {
            token(kind);
            at_ += length;
            current_->temporal = true;
            return true;
        };
        const std::size_t length = run_length(lowered);
        switch (lowered) {
        case 'y':
            current_->has_date = true;
            return add(length <= 2 ? token_kind::year_2 : token_kind::year_4,
                       length);
        case 'd':
            current_->has_date = true;
            return add(length == 1   ? token_kind::day_1
                       : length == 2 ? token_kind::day_2
                       : length == 3 ? token_kind::day_abbrev
                                     : token_kind::day_name,
                       length);
        case 'h':
            current_->has_time = true;
            return add(length == 1 ? token_kind::hour_1 : token_kind::hour_2,
                       length);
        case 's':
            current_->has_time = true;
            return add(length == 1 ? token_kind::second_1
                                   : token_kind::second_2,
                       length);
        case 'm':
            current_->has_date = true;
            return add(length == 1   ? token_kind::month_1
                       : length == 2 ? token_kind::month_2
                       : length == 3 ? token_kind::month_abbrev
                       : length == 4 ? token_kind::month_name
                                     : token_kind::month_letter,
                       length);
        default:
            return false;
        }
    }

    // Consume one element of the section.
    void step(char c) {
        switch (c) {
        case '"': {
            const auto close = text_.find('"', at_ + 1);
            const auto end = close == std::string_view::npos ? text_.size()
                                                             : close;
            literal(text_.substr(at_ + 1, end - at_ - 1));
            at_ = end + 1;
            return;
        }
        case '\\':
            if (at_ + 1 < text_.size())
                literal(text_.substr(at_ + 1, 1));
            at_ += 2;
            return;
        case '_':
            literal(" ");
            at_ += 2;
            return;
        case '*':
            at_ += 2;
            return;
        case '[':
            bracket();
            return;
        case '0':
        case '#':
        case '?':
            digit(c);
            return;
        case '.':
            if (at_ + 1 < text_.size() && text_[at_ + 1] == '0' &&
                current_->temporal) {
                int size = 0;
                ++at_;
                while (at_ < text_.size() && text_[at_] == '0') {
                    ++size;
                    ++at_;
                }
                token(token_kind::second_fraction, size);
                return;
            }
            if (in_decimals_) {
                literal(".");
            } else {
                token(token_kind::decimal);
                in_decimals_ = true;
            }
            ++at_;
            return;
        case ',':
            comma();
            return;
        case '%':
            ++current_->percents;
            literal("%");
            ++at_;
            return;
        case '@':
            token(token_kind::text_body);
            current_->has_text = true;
            ++at_;
            return;
        case '/':
            slash();
            return;
        case 'E':
        case 'e':
            if (at_ + 1 < text_.size() &&
                (text_[at_ + 1] == '+' || text_[at_ + 1] == '-') &&
                current_->has_digits) {
                current_->exponent_plus = text_[at_ + 1] == '+';
                token(token_kind::exponent);
                in_exponent_ = true;
                at_ += 2;
                return;
            }
            break;
        case '$':
            current_->has_currency = true;
            break;
        default:
            break;
        }
        if (looking_at("general")) {
            current_->general = true;
            at_ += 7;
            return;
        }
        if (looking_at("am/pm") || looking_at("a/p")) {
            const std::size_t length = looking_at("am/pm") ? 5 : 3;
            current_->tokens.push_back(
                {token_kind::am_pm, std::string(text_.substr(at_, length)),
                 0});
            current_->temporal = true;
            at_ += length;
            return;
        }
        const char lowered = static_cast<char>(c | 0x20);
        if (c >= 'A' && c <= 'z' && temporal_letter(lowered))
            return;
        // A whole UTF-8 sequence stays one literal.
        std::size_t length = 1;
        while (at_ + length < text_.size() &&
               (static_cast<unsigned char>(text_[at_ + length]) & 0xC0) ==
                   0x80)
            ++length;
        const std::string_view piece = text_.substr(at_, length);
        if (piece == "\xE2\x82\xAC" || piece == "\xC2\xA3" ||
            piece == "\xC2\xA5")
            current_->has_currency = true;
        literal(piece);
        at_ += length;
    }

    // A comma between digits groups thousands; before the point or the
    // end it divides by a thousand.
    void comma() {
        std::size_t next = at_;
        while (next < text_.size() && text_[next] == ',')
            ++next;
        const bool digit_follows =
            next < text_.size() && (text_[next] == '0' || text_[next] == '#' ||
                                    text_[next] == '?');
        if (current_->has_digits && !in_decimals_ && digit_follows)
            current_->thousands = true;
        else if (current_->has_digits)
            current_->scale_commas += static_cast<int>(next - at_);
        else
            literal(text_.substr(at_, next - at_));
        at_ = next;
    }

    // A slash after digits starts a fraction's denominator.
    void slash() {
        if (!current_->has_digits || in_decimals_ || current_->temporal) {
            literal("/");
            ++at_;
            return;
        }
        ++at_;
        current_->fraction = true;
        token(token_kind::fraction_bar);
        std::size_t end = at_;
        while (end < text_.size() && text_[end] >= '1' && text_[end] <= '9')
            ++end;
        while (end > at_ && end < text_.size() && text_[end] == '0')
            ++end;
        if (end > at_) {
            std::from_chars(text_.data() + at_, text_.data() + end,
                            current_->fixed_denominator);
            at_ = end;
        } else {
            in_denominator_ = true;
        }
        // The digits before the bar are the numerator; split a mixed
        // number's whole part from them at the last literal.
        auto &tokens = current_->tokens;
        int numerator = 0;
        for (auto i = tokens.size() - 1; i-- > 0;) {
            const token_kind kind = tokens[i].kind;
            if (kind != token_kind::digit_zero &&
                kind != token_kind::digit_hash &&
                kind != token_kind::digit_blank)
                break;
            ++numerator;
        }
        current_->numerator_digits = numerator;
        current_->integer_digits -= numerator;
    }

    // Resolve the context left over from scanning.
    void finish(format_section &section) {
        auto &tokens = section.tokens;
        // "m" next to hours or seconds means minutes.
        const auto is_minute_context = [](token_kind kind) {
            return kind == token_kind::hour_1 || kind == token_kind::hour_2 ||
                   kind == token_kind::elapsed_hours;
        };
        const auto is_second = [](token_kind kind) {
            return kind == token_kind::second_1 ||
                   kind == token_kind::second_2 ||
                   kind == token_kind::elapsed_seconds;
        };
        token_kind previous = token_kind::literal;
        for (std::size_t i = 0; i < tokens.size(); ++i) {
            const token_kind kind = tokens[i].kind;
            if (kind != token_kind::month_1 && kind != token_kind::month_2) {
                if (kind != token_kind::literal)
                    previous = kind;
                continue;
            }
            bool minute = is_minute_context(previous);
            for (std::size_t j = i + 1; j < tokens.size() && !minute; ++j) {
                if (tokens[j].kind == token_kind::literal)
                    continue;
                minute = is_second(tokens[j].kind);
                break;
            }
            if (minute) {
                tokens[i].kind = kind == token_kind::month_1
                                     ? token_kind::minute_1
                                     : token_kind::minute_2;
                section.has_time = true;
            }
            previous = tokens[i].kind;
        }
        section.has_date = false;
        for (const format_token &item : tokens)
            switch (item.kind) {
            case token_kind::year_2:
            case token_kind::year_4:
            case token_kind::month_1:
            case token_kind::month_2:
            case token_kind::month_abbrev:
            case token_kind::month_name:
            case token_kind::month_letter:
            case token_kind::day_1:
            case token_kind::day_2:
            case token_kind::day_abbrev:
            case token_kind::day_name:
                section.has_date = true;
                break;
            default:
                break;
            }
    }

    std::string_view text_;
    std::size_t at_ = 0;
    format_section *current_ = nullptr;
    bool in_decimals_ = false;
    bool in_exponent_ = false;
    bool in_denominator_ = false;
};

} // namespace

bool section_condition::matches(double value) const {
    switch (comparison) {
    case op::lt:
        return value < limit;
    case op::le:
        return value <= limit;
    case op::gt:
        return value > limit;
    case op::ge:
        return value >= limit;
    case op::eq:
        return value == limit;
    case op::ne:
        return value != limit;
    }
    return false;
}

std::vector<format_section> parse_sections(std::string_view source) {
    return scanner(source).run();
}

} // namespace numfmt
