//
// Renders values through parsed formats: section choice, General, the
// numeric layout of digits around literals, and dates.
//
// Derived from goffice go-format.c,
// Copyright (C) 2005-2015 Morten Welinder, Jody Goldberg and contributors.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include "numfmt/format.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <map>

#include "format_section.h"
#include "numfmt/date.h"
#include "util/text.h"

namespace numfmt {
namespace {

constexpr std::array<std::string_view, 12> month_names{
    "January", "February", "March",     "April",   "May",      "June",
    "July",    "August",   "September", "October", "November", "December"};
constexpr std::array<std::string_view, 7> day_names{
    "Sunday",   "Monday", "Tuesday", "Wednesday",
    "Thursday", "Friday", "Saturday"};

// Remove trailing zeros of a plain or exponent number.
std::string strip_zeros(std::string text) {
    const auto exponent = text.find_first_of("eE");
    std::string tail;
    if (exponent != std::string::npos) {
        tail = text.substr(exponent);
        text.resize(exponent);
    }
    if (text.find('.') != std::string::npos) {
        while (text.back() == '0')
            text.pop_back();
        if (text.back() == '.')
            text.pop_back();
    }
    return text + tail;
}

// Return %g-like text with the exponent written E+nn.
std::string with_precision(double value, int digits) {
    char buffer[64];
    std::snprintf(buffer, sizeof buffer, "%.*g", digits, value);
    std::string text = strip_zeros(buffer);
    const auto mark = text.find('e');
    if (mark != std::string::npos)
        text[mark] = 'E';
    return text;
}

// Return the integer part's width in characters, sign included.
int integer_width(double value) {
    const double magnitude = std::fabs(value);
    const int digits =
        magnitude < 1 ? 1 : static_cast<int>(std::log10(magnitude)) + 1;
    return digits + (value < 0 ? 1 : 0);
}

// Return text in scientific notation with a number of mantissa digits.
std::string scientific(double value, int digits) {
    char buffer[64];
    std::snprintf(buffer, sizeof buffer, "%.*E", digits, value);
    return strip_zeros(buffer);
}

// Pick the section a number renders through.
const format_section *
choose_section(const std::vector<format_section> &sections, double value,
               bool &strip_sign) {
    const std::size_t count = sections.size();
    strip_sign = false;
    bool conditioned = false;
    for (const format_section &section : sections)
        conditioned = conditioned || section.condition.has_value();
    if (conditioned) {
        for (std::size_t i = 0; i < count && i < 3; ++i) {
            const format_section &section = sections[i];
            if (section.has_text && !section.has_digits && !section.general)
                continue;
            if (!section.condition || section.condition->matches(value)) {
                strip_sign = i > 0 || section.condition.has_value();
                return &section;
            }
        }
        return nullptr;
    }
    if (count == 1)
        return &sections[0];
    if (count == 2) {
        strip_sign = value < 0;
        return value >= 0 ? &sections[0] : &sections[1];
    }
    if (value > 0)
        return &sections[0];
    if (value < 0) {
        strip_sign = true;
        return &sections[1];
    }
    return &sections[2];
}

// Classify a parsed format.
format_family classify(const std::vector<format_section> &sections) {
    if (sections.empty())
        return format_family::general;
    const format_section &first = sections[0];
    if (first.general)
        return format_family::general;
    if (first.temporal)
        return first.has_date ? format_family::date : format_family::time;
    if (first.has_text && !first.has_digits)
        return format_family::text;
    if (first.fraction)
        return format_family::fraction;
    if (first.exponent_digits > 0)
        return format_family::scientific;
    if (first.percents > 0)
        return format_family::percent;
    if (first.has_currency)
        return format_family::currency;
    if (first.has_digits)
        return format_family::number;
    return format_family::other;
}

// Return the numerator and denominator closest to a fraction.
void best_fraction(double value, int max_denominator, int &numerator,
                   int &denominator) {
    int lower_n = 0, lower_d = 1, upper_n = 1, upper_d = 0;
    numerator = 0;
    denominator = 1;
    double best = value;
    while (true) {
        const int middle_n = lower_n + upper_n;
        const int middle_d = lower_d + upper_d;
        if (middle_d > max_denominator)
            break;
        const double middle = static_cast<double>(middle_n) / middle_d;
        if (std::fabs(middle - value) < best) {
            best = std::fabs(middle - value);
            numerator = middle_n;
            denominator = middle_d;
        }
        if (middle < value) {
            lower_n = middle_n;
            lower_d = middle_d;
        } else if (middle > value) {
            upper_n = middle_n;
            upper_d = middle_d;
        } else {
            break;
        }
    }
}

// Emits digits into the placeholders of one group of tokens.
class digit_writer {
public:
    // Prepare to write a digit string right-aligned in placeholders.
    digit_writer(std::string digits, int placeholders, bool group)
        : digits_(std::move(digits)), placeholders_(placeholders),
          group_(group) {
        remaining_ = placeholders_;
    }

    // Write the next placeholder.
    void write(token_kind kind, std::string &out) {
        const int have = static_cast<int>(digits_.size());
        const int position = remaining_ - 1;
        if (remaining_ == placeholders_ && have > placeholders_) {
            for (int i = 0; i < have - placeholders_; ++i) {
                out.push_back(digits_[i]);
                separator(have - 1 - i, out);
            }
        }
        if (position < have) {
            out.push_back(digits_[have - 1 - position]);
            separator(position, out);
        } else if (kind == token_kind::digit_zero) {
            out.push_back('0');
            separator(position, out);
        } else if (kind == token_kind::digit_blank) {
            out.push_back(' ');
        }
        --remaining_;
    }

private:
    // Write a comma after a digit at this place when grouping.
    void separator(int place, std::string &out) const {
        if (group_ && place > 0 && place % 3 == 0)
            out.push_back(',');
    }

    std::string digits_;
    int placeholders_;
    int remaining_ = 0;
    bool group_;
};

// Return decimal text with a whole and a decimal part.
void split_number(double value, int decimals, std::string &whole,
                  std::string &fraction) {
    char buffer[400];
    std::snprintf(buffer, sizeof buffer, "%.*f", decimals, value);
    const std::string text = buffer;
    const auto point = text.find('.');
    whole = text.substr(0, point);
    fraction = point == std::string::npos ? "" : text.substr(point + 1);
    if (whole == "0")
        whole.clear();
}

// Render tokens holding digits around a fraction bar.
std::string render_fraction(const format_section &section, double value) {
    const bool mixed = section.integer_digits > 0;
    const double whole = mixed ? std::floor(value) : 0;
    const double part = value - whole;
    int numerator = 0;
    int denominator = 1;
    if (section.fixed_denominator > 0) {
        denominator = section.fixed_denominator;
        numerator = static_cast<int>(std::lround(part * denominator));
    } else {
        int limit = 1;
        for (int i = 0; i < section.denominator_digits; ++i)
            limit *= 10;
        best_fraction(part, limit - 1, numerator, denominator);
    }
    double shown_whole = whole;
    if (mixed && numerator >= denominator) {
        shown_whole += 1;
        numerator -= denominator;
    }
    const std::string whole_text =
        shown_whole != 0 ? with_precision(shown_whole, 15) : "";
    digit_writer integers(whole_text, section.integer_digits, false);
    digit_writer tops(numerator ? std::to_string(numerator) : "",
                      section.numerator_digits, false);
    digit_writer bottoms(numerator ? std::to_string(denominator) : "",
                         section.denominator_digits, false);
    std::string out;
    int integer_seen = 0;
    bool after_bar = false;
    for (const format_token &item : section.tokens)
        switch (item.kind) {
        case token_kind::digit_zero:
        case token_kind::digit_hash:
        case token_kind::digit_blank:
            if (after_bar)
                bottoms.write(item.kind, out);
            else if (integer_seen < section.integer_digits) {
                ++integer_seen;
                integers.write(item.kind, out);
            } else {
                tops.write(item.kind, out);
            }
            break;
        case token_kind::fraction_bar:
            after_bar = true;
            if (numerator == 0) {
                out.append(1 + section.denominator_digits, ' ');
            } else {
                out.push_back('/');
                if (section.fixed_denominator > 0)
                    out += std::to_string(denominator);
            }
            break;
        case token_kind::literal:
            out += item.text;
            break;
        default:
            break;
        }
    if (shown_whole == 0 && numerator == 0)
        return "0";
    return out;
}

// Render tokens holding an exponent.
std::string render_exponent(const format_section &section, double value) {
    int exponent = 0;
    if (value != 0) {
        exponent = static_cast<int>(std::floor(std::log10(value)));
        const int step = std::max(section.integer_digits, 1);
        if (step > 1)
            exponent = static_cast<int>(
                           std::floor(static_cast<double>(exponent) / step)) *
                       step;
        value /= std::pow(10.0, exponent);
        // Rounding the mantissa can reach the next power.
        const double limit =
            std::pow(10.0, std::max(section.integer_digits, 1));
        if (std::round(value * std::pow(10.0, section.decimal_digits)) /
                std::pow(10.0, section.decimal_digits) >=
            limit) {
            value /= 10;
            ++exponent;
        }
    }
    std::string whole;
    std::string fraction;
    split_number(value, section.decimal_digits, whole, fraction);
    digit_writer integers(whole, section.integer_digits, false);
    digit_writer powers(std::to_string(std::abs(exponent)),
                        section.exponent_digits, false);
    std::string out;
    bool after_point = false;
    bool after_exponent = false;
    std::size_t decimal_at = 0;
    for (const format_token &item : section.tokens)
        switch (item.kind) {
        case token_kind::digit_zero:
        case token_kind::digit_hash:
        case token_kind::digit_blank:
            if (after_exponent)
                powers.write(item.kind, out);
            else if (after_point) {
                const char digit =
                    decimal_at < fraction.size() ? fraction[decimal_at] : '0';
                ++decimal_at;
                out.push_back(digit);
            } else {
                integers.write(item.kind, out);
            }
            break;
        case token_kind::decimal:
            after_point = true;
            out.push_back('.');
            break;
        case token_kind::exponent:
            after_exponent = true;
            out.push_back('E');
            if (exponent < 0)
                out.push_back('-');
            else if (section.exponent_plus)
                out.push_back('+');
            break;
        case token_kind::literal:
            out += item.text;
            break;
        default:
            break;
        }
    return out;
}

// Return a section's value scaled by its percent signs and commas.
double scaled(const format_section &section, double value) {
    for (int i = 0; i < section.percents; ++i)
        value *= 100;
    for (int i = 0; i < section.scale_commas; ++i)
        value /= 1000;
    return value;
}

} // namespace

std::string render_numeric(const format_section &section, double value) {
    value = scaled(section, value);
    if (section.fraction)
        return render_fraction(section, value);
    if (section.exponent_digits > 0)
        return render_exponent(section, value);
    std::string whole;
    std::string fraction;
    split_number(value, section.decimal_digits, whole, fraction);
    digit_writer integers(whole, section.integer_digits, section.thousands);
    // Trailing # decimals drop their zeros.
    std::vector<token_kind> kinds;
    bool after_point = false;
    for (const format_token &item : section.tokens) {
        if (item.kind == token_kind::decimal)
            after_point = true;
        else if (after_point && (item.kind == token_kind::digit_zero ||
                                 item.kind == token_kind::digit_hash ||
                                 item.kind == token_kind::digit_blank))
            kinds.push_back(item.kind);
    }
    std::size_t shown = kinds.size();
    while (shown > 0 && kinds[shown - 1] != token_kind::digit_zero &&
           fraction[shown - 1] == '0')
        --shown;
    std::string out;
    after_point = false;
    std::size_t decimal_at = 0;
    for (const format_token &item : section.tokens)
        switch (item.kind) {
        case token_kind::digit_zero:
        case token_kind::digit_hash:
        case token_kind::digit_blank:
            if (!after_point) {
                integers.write(item.kind, out);
            } else {
                if (decimal_at < shown)
                    out.push_back(fraction[decimal_at]);
                else if (item.kind == token_kind::digit_blank)
                    out.push_back(' ');
                ++decimal_at;
            }
            break;
        case token_kind::decimal:
            after_point = true;
            if (shown > 0 || section.decimal_digits == 0 ||
                section.integer_digits == 0)
                out.push_back('.');
            break;
        case token_kind::literal:
            out += item.text;
            break;
        default:
            break;
        }
    return out;
}

std::string render_temporal(const format_section &section, double value) {
    const auto date = date_from_serial(value);
    if (!date)
        return std::string(10, '#');
    // Round the fraction to what the section shows, carrying into the
    // date so 23:59:59.9 under h:mm:ss does not print 60 seconds.
    int second_digits = 0;
    for (const format_token &item : section.tokens)
        if (item.kind == token_kind::second_fraction)
            second_digits = item.size;
    const double unit = 86400.0 * std::pow(10.0, second_digits);
    double serial = std::round(value * unit) / unit;
    const auto carried = date_from_serial(serial);
    const civil_date day = carried ? *carried : *date;
    const civil_time time = time_from_serial(serial);
    bool twelve_hours = false;
    for (const format_token &item : section.tokens)
        twelve_hours = twelve_hours || item.kind == token_kind::am_pm;
    const int shown_hour =
        twelve_hours ? (time.hour % 12 == 0 ? 12 : time.hour % 12) : time.hour;
    const auto two = [](int number) {
        char buffer[16];
        std::snprintf(buffer, sizeof buffer, "%02d", number);
        return std::string(buffer);
    };
    const int weekday = weekday_from_serial(serial);
    std::string out;
    double whole_second = std::floor(time.second);
    for (const format_token &item : section.tokens)
        switch (item.kind) {
        case token_kind::literal:
            out += item.text;
            break;
        case token_kind::year_2:
            out += two(day.year % 100);
            break;
        case token_kind::year_4:
            out += std::to_string(day.year);
            break;
        case token_kind::month_1:
            out += std::to_string(day.month);
            break;
        case token_kind::month_2:
            out += two(day.month);
            break;
        case token_kind::month_abbrev:
            out += month_names[day.month - 1].substr(0, 3);
            break;
        case token_kind::month_name:
            out += month_names[day.month - 1];
            break;
        case token_kind::month_letter:
            out += month_names[day.month - 1].substr(0, 1);
            break;
        case token_kind::day_1:
            out += std::to_string(day.day);
            break;
        case token_kind::day_2:
            out += two(day.day);
            break;
        case token_kind::day_abbrev:
            out += day_names[weekday].substr(0, 3);
            break;
        case token_kind::day_name:
            out += day_names[weekday];
            break;
        case token_kind::hour_1:
            out += std::to_string(shown_hour);
            break;
        case token_kind::hour_2:
            out += two(shown_hour);
            break;
        case token_kind::minute_1:
            out += std::to_string(time.minute);
            break;
        case token_kind::minute_2:
            out += two(time.minute);
            break;
        case token_kind::second_1:
            out += std::to_string(static_cast<int>(whole_second));
            break;
        case token_kind::second_2:
            out += two(static_cast<int>(whole_second));
            break;
        case token_kind::elapsed_hours:
            out += std::to_string(static_cast<long long>(
                std::floor(serial * 24 + 1e-9)));
            break;
        case token_kind::elapsed_minutes:
            out += std::to_string(static_cast<long long>(
                std::floor(serial * 1440 + 1e-9)));
            break;
        case token_kind::elapsed_seconds:
            out += std::to_string(
                static_cast<long long>(std::round(serial * 86400)));
            break;
        case token_kind::second_fraction: {
            char buffer[32];
            std::snprintf(buffer, sizeof buffer, "%.*f", item.size,
                          time.second - whole_second);
            out += buffer + 1;
            break;
        }
        case token_kind::am_pm: {
            const bool pm = time.hour >= 12;
            const bool lower = item.text[0] == 'a' || item.text[0] == 'p';
            std::string word = item.text.size() == 5
                                   ? (pm ? "PM" : "AM")
                                   : (pm ? "P" : "A");
            out += lower ? util::to_lower(word) : word;
            break;
        }
        default:
            break;
        }
    return out;
}

std::string render_general(double value, int max_chars) {
    if (std::isnan(value))
        return "#NUM!";
    if (std::isinf(value))
        return value > 0 ? "Infinity" : "-Infinity";
    if (value == 0)
        return "0";
    const double magnitude = std::fabs(value);
    if (max_chars < 0) {
        if (magnitude >= 1e-10 && magnitude < 1e15) {
            char buffer[64];
            std::snprintf(buffer, sizeof buffer, "%.15g", value);
            std::string text = buffer;
            if (text.find('e') == std::string::npos)
                return text;
        }
        return with_precision(value, 15);
    }
    // Integers keep every digit when they fit; otherwise reduce the
    // precision until the plain form fits, then use an exponent.
    if (magnitude >= 1e-4 && magnitude < 1e15) {
        const int whole = integer_width(value);
        if (whole <= max_chars) {
            const int room = std::min(15, std::max(1, max_chars -
                                                          (value < 0 ? 1 : 0)));
            for (int digits = room; digits >= 1; --digits) {
                char buffer[64];
                std::snprintf(buffer, sizeof buffer, "%.*f",
                              std::max(0, digits - (whole - (value < 0))),
                              value);
                std::string text = strip_zeros(buffer);
                if (static_cast<int>(text.size()) <= max_chars)
                    return text;
            }
        }
    }
    for (int digits = 14; digits >= 0; --digits) {
        const std::string text = scientific(value, digits);
        if (static_cast<int>(text.size()) <= max_chars || digits == 0)
            return text;
    }
    return with_precision(value, 15);
}

number_format::number_format(std::string source,
                             std::vector<format_section> sections)
    : source_(std::move(source)), sections_(std::move(sections)),
      family_(classify(sections_)) {}

number_format::~number_format() = default;

std::shared_ptr<const number_format>
number_format::get(std::string_view source) {
    static std::map<std::string, std::shared_ptr<const number_format>,
                    std::less<>>
        known;
    const auto found = known.find(source);
    if (found != known.end())
        return found->second;
    std::shared_ptr<const number_format> created(new number_format(
        std::string(source), parse_sections(source)));
    known.emplace(std::string(source), created);
    return created;
}

std::shared_ptr<const number_format> number_format::general() {
    static const auto shared = get("General");
    return shared;
}

rendered number_format::render(double value, int max_chars) const {
    bool strip_sign = false;
    const format_section *section =
        choose_section(sections_, value, strip_sign);
    rendered result;
    if (!section) {
        result.text = std::string(5, '#');
        return result;
    }
    result.color = section->color;
    if (section->general || sections_.empty()) {
        result.text = render_general(strip_sign ? std::fabs(value) : value,
                                     max_chars);
        for (const format_token &item : section->tokens)
            if (item.kind == token_kind::literal)
                result.text += item.text;
        return result;
    }
    if (section->temporal) {
        result.text = render_temporal(*section, value);
        return result;
    }
    if (!section->has_digits) {
        for (const format_token &item : section->tokens)
            if (item.kind == token_kind::literal)
                result.text += item.text;
            else if (item.kind == token_kind::text_body)
                result.text += render_general(value);
        return result;
    }
    const std::string body = render_numeric(*section, std::fabs(value));
    const bool negative = value < 0 && !strip_sign;
    bool all_zero = true;
    for (const char c : body)
        all_zero = all_zero && (c < '1' || c > '9');
    result.text = negative && !all_zero ? "-" + body : body;
    return result;
}

rendered number_format::render_text(std::string_view text) const {
    rendered result;
    const format_section *section = nullptr;
    if (sections_.size() >= 4)
        section = &sections_[3];
    else
        for (const format_section &candidate : sections_)
            if (candidate.has_text)
                section = &candidate;
    if (!section) {
        result.text = text;
        return result;
    }
    result.color = section->color;
    for (const format_token &item : section->tokens)
        if (item.kind == token_kind::literal)
            result.text += item.text;
        else if (item.kind == token_kind::text_body)
            result.text += text;
    return result;
}

} // namespace numfmt
