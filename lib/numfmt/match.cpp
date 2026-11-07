//
// Recognizers run in order from the most to the least specific; the
// first that consumes the entire entry wins. Dates follow the US
// month-first order, which is the C locale Gnumeric writes files in.
//
// Derived from Gnumeric src/number-match.c,
// Copyright (C) 1999 Miguel de Icaza, 2006 Morten Welinder.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include "numfmt/match.h"

#include <array>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <ctime>

#include "numfmt/date.h"
#include "util/text.h"

namespace numfmt {
namespace {

constexpr std::array<std::string_view, 12> month_words{
    "jan", "feb", "mar", "apr", "may", "jun",
    "jul", "aug", "sep", "oct", "nov", "dec"};

// Walks an entry left to right.
class cursor {
public:
    explicit cursor(std::string_view text) : text_(text) {}

    bool done() const { return at_ >= text_.size(); }

    char peek() const { return done() ? '\0' : text_[at_]; }

    std::size_t position() const { return at_; }

    void seek(std::size_t position) { at_ = position; }

    // Skip spaces.
    void spaces() {
        while (peek() == ' ')
            ++at_;
    }

    // Consume a character when it is next.
    bool take(char c) {
        if (peek() != c)
            return false;
        ++at_;
        return true;
    }

    // Consume digits; nothing when none are there.
    std::optional<int> integer(int *length = nullptr) {
        const std::size_t start = at_;
        int value = 0;
        while (peek() >= '0' && peek() <= '9') {
            if (value < 100000000)
                value = value * 10 + (text_[at_] - '0');
            ++at_;
        }
        if (at_ == start)
            return std::nullopt;
        if (length)
            *length = static_cast<int>(at_ - start);
        return value;
    }

    // Consume a month name and return its number.
    std::optional<int> month() {
        std::size_t end = at_;
        while (end < text_.size() &&
               ((text_[end] | 0x20) >= 'a' && (text_[end] | 0x20) <= 'z'))
            ++end;
        if (end - at_ < 3)
            return std::nullopt;
        const std::string word =
            util::to_lower(text_.substr(at_, end - at_));
        for (std::size_t i = 0; i < month_words.size(); ++i) {
            if (word.rfind(month_words[i], 0) == 0) {
                static constexpr std::array<std::string_view, 12> full{
                    "january", "february", "march",     "april",
                    "may",     "june",     "july",      "august",
                    "september", "october", "november", "december"};
                if (word.size() == 3 || full[i] == word ||
                    (i == 8 && word == "sept")) {
                    at_ = end;
                    return static_cast<int>(i) + 1;
                }
            }
        }
        return std::nullopt;
    }

    std::string_view rest() const { return text_.substr(at_); }

private:
    std::string_view text_;
    std::size_t at_ = 0;
};

// Return the current year, for dates entered without one.
int this_year() {
    const std::time_t now = std::time(nullptr);
    std::tm parts{};
#if defined(_WIN32)
    gmtime_s(&parts, &now);
#else
    gmtime_r(&now, &parts);
#endif
    return parts.tm_year + 1900;
}

// Expand a two-digit year; thirty and up are the last century.
int full_year(int year, int digits) {
    if (digits > 2)
        return year;
    return year < 30 ? 2000 + year : 1900 + year;
}

// Report whether a date is real.
bool valid_date(int year, int month, int day) {
    return year >= 1900 && year <= 9999 && month >= 1 && month <= 12 &&
           day >= 1 && day <= days_in_month(year, month);
}

// Recognize a time at the cursor, returning it as a day fraction.
std::optional<double> scan_time(cursor &in, std::string &format) {
    const std::size_t start = in.position();
    const auto hour = in.integer();
    if (!hour || !in.take(':')) {
        in.seek(start);
        return std::nullopt;
    }
    const auto minute = in.integer();
    if (!minute || *minute > 59) {
        in.seek(start);
        return std::nullopt;
    }
    double seconds = 0;
    bool has_seconds = false;
    if (in.take(':')) {
        const auto whole = in.integer();
        if (!whole || *whole > 59) {
            in.seek(start);
            return std::nullopt;
        }
        has_seconds = true;
        seconds = *whole;
        if (in.take('.')) {
            int length = 0;
            const auto part = in.integer(&length);
            if (part)
                seconds += *part / std::pow(10.0, length);
        }
    }
    in.spaces();
    int hours = *hour;
    bool meridian = false;
    const std::string tail = util::to_lower(in.rest());
    for (const std::string_view word : {"am", "pm", "a", "p"})
        if (tail == word) {
            meridian = true;
            if (hours < 1 || hours > 12) {
                in.seek(start);
                return std::nullopt;
            }
            hours %= 12;
            if (word[0] == 'p')
                hours += 12;
            in.seek(in.position() + word.size());
            break;
        }
    format = has_seconds ? "h:mm:ss" : "h:mm";
    if (meridian)
        format += " AM/PM";
    return (hours * 3600.0 + *minute * 60.0 + seconds) / 86400.0;
}

// Recognize a date at the cursor.
std::optional<double> scan_date(cursor &in, std::string &format) {
    const std::size_t start = in.position();
    const auto fail = [&]() -> std::optional<double> {
        in.seek(start);
        return std::nullopt;
    };
    // Dec 25, 2024 and December 2024.
    if (const auto named = in.month()) {
        in.spaces();
        in.take('-');
        int digits = 0;
        const auto day = in.integer(&digits);
        if (!day)
            return fail();
        if (digits == 4 && valid_date(*day, *named, 1)) {
            format = "mmm yyyy";
            return serial_from_date(*day, *named, 1);
        }
        in.spaces();
        const bool comma = in.take(',');
        in.spaces();
        if (in.take('-'))
            in.spaces();
        int year_digits = 4;
        const auto year = in.integer(&year_digits);
        const int full = year ? full_year(*year, year_digits) : this_year();
        if ((comma && !year) || !valid_date(full, *named, *day))
            return fail();
        format = "d-mmm-yyyy";
        return serial_from_date(full, *named, *day);
    }
    int first_digits = 0;
    const auto first = in.integer(&first_digits);
    if (!first)
        return fail();
    const char separator = in.peek();
    if (separator == '-' || separator == ' ') {
        // 25-Dec-2024 and 25 Dec 2024.
        const std::size_t mark = in.position();
        in.take(separator);
        if (const auto named = in.month()) {
            if (in.peek() != separator && !in.done())
                return fail();
            in.take(separator);
            int digits = 4;
            const auto year = in.integer(&digits);
            const int full = year ? full_year(*year, digits) : this_year();
            if (!valid_date(full, *named, *first))
                return fail();
            format = "d-mmm-yyyy";
            return serial_from_date(full, *named, *first);
        }
        in.seek(mark);
    }
    if (separator != '/' && separator != '-')
        return fail();
    in.take(separator);
    int second_digits = 0;
    const auto second = in.integer(&second_digits);
    if (!second)
        return fail();
    // 2024-12-25: a leading four-digit year.
    if (first_digits == 4) {
        if (!in.take(separator))
            return fail();
        const auto day = in.integer();
        if (!day || !valid_date(*first, *second, *day))
            return fail();
        format = "yyyy-mm-dd";
        return serial_from_date(*first, *second, *day);
    }
    if (!in.take(separator)) {
        // 12/25: this year. 12/2024: that month.
        if (second_digits == 4 && valid_date(*second, *first, 1)) {
            format = "mmm yyyy";
            return serial_from_date(*second, *first, 1);
        }
        const int year = this_year();
        if (separator != '/' || !valid_date(year, *first, *second))
            return fail();
        format = "d-mmm";
        return serial_from_date(year, *first, *second);
    }
    int year_digits = 0;
    const auto year = in.integer(&year_digits);
    if (!year)
        return fail();
    const int full = full_year(*year, year_digits);
    if (!valid_date(full, *first, *second))
        return fail();
    format = "m/d/yyyy";
    return serial_from_date(full, *first, *second);
}

// Recognize dates, times and date-times.
std::optional<number_match> match_temporal(std::string_view text) {
    cursor in(text);
    std::string format;
    if (const auto time = scan_time(in, format)) {
        if (in.done())
            return number_match{*time, format};
        return std::nullopt;
    }
    const auto date = scan_date(in, format);
    if (!date)
        return std::nullopt;
    if (in.done())
        return number_match{*date, format};
    in.spaces();
    std::string time_format;
    const auto time = scan_time(in, time_format);
    if (!time || !in.done())
        return std::nullopt;
    return number_match{*date + *time, format + " " + time_format};
}

// Parse a plain number; grouping is checked against thousands places.
std::optional<double> plain_number(std::string_view text, bool &grouped,
                                   int &decimals) {
    std::string cleaned;
    std::size_t at = 0;
    grouped = false;
    decimals = 0;
    int run = 0;
    bool seen_comma = false;
    bool seen_point = false;
    bool seen_digit = false;
    for (; at < text.size(); ++at) {
        const char c = text[at];
        if (c >= '0' && c <= '9') {
            cleaned.push_back(c);
            seen_digit = true;
            ++run;
            if (seen_point)
                ++decimals;
        } else if (c == ',' && !seen_point && seen_digit &&
                   (!seen_comma || run == 3)) {
            if (seen_comma && run != 3)
                return std::nullopt;
            seen_comma = true;
            grouped = true;
            run = 0;
        } else if (c == '.' && !seen_point) {
            if (seen_comma && run != 3)
                return std::nullopt;
            seen_point = true;
            cleaned.push_back(c);
        } else {
            break;
        }
    }
    if (!seen_digit || (seen_comma && !seen_point && run != 3))
        return std::nullopt;
    if (at < text.size() && (text[at] == 'e' || text[at] == 'E')) {
        std::size_t next = at + 1;
        if (next < text.size() && (text[next] == '+' || text[next] == '-'))
            ++next;
        const std::size_t digits = next;
        while (next < text.size() && text[next] >= '0' && text[next] <= '9')
            ++next;
        if (next == digits)
            return std::nullopt;
        cleaned += text.substr(at, next - at);
        at = next;
    }
    if (at != text.size())
        return std::nullopt;
    return std::strtod(cleaned.c_str(), nullptr);
}

// Recognize numbers with sign, currency, percent and parentheses.
std::optional<number_match> match_decimal(std::string_view text) {
    bool negative = false;
    std::string symbol;
    bool parenthesized = false;
    if (text.size() > 2 && text.front() == '(' && text.back() == ')') {
        parenthesized = true;
        negative = true;
        text = text.substr(1, text.size() - 2);
    }
    const auto take_sign = [&] {
        if (!text.empty() && (text[0] == '-' || text[0] == '+')) {
            negative = text[0] == '-' ? !negative : negative;
            text.remove_prefix(1);
            return true;
        }
        return false;
    };
    const auto take_currency = [&] {
        for (const std::string_view mark :
             {"$", "\xE2\x82\xAC", "\xC2\xA3", "\xC2\xA5"})
            if (text.substr(0, mark.size()) == mark) {
                symbol = mark;
                text.remove_prefix(mark.size());
                return;
            }
    };
    take_sign();
    take_currency();
    take_sign();
    text = util::trim(text);
    bool percent = false;
    if (!text.empty() && text.back() == '%') {
        percent = true;
        text.remove_suffix(1);
        text = util::trim(text);
    }
    if (symbol.empty())
        for (const std::string_view mark : {"\xE2\x82\xAC", "\xC2\xA3"})
            if (text.size() > mark.size() &&
                text.substr(text.size() - mark.size()) == mark) {
                symbol = mark;
                text.remove_suffix(mark.size());
                text = util::trim(text);
                break;
            }
    bool grouped = false;
    int decimals = 0;
    const auto parsed = plain_number(text, grouped, decimals);
    if (!parsed)
        return std::nullopt;
    number_match result;
    result.value = negative ? -*parsed : *parsed;
    if (percent) {
        result.value /= 100;
        result.format = decimals > 0 ? "0.00%" : "0%";
    } else if (!symbol.empty()) {
        result.format = symbol + "#,##0.00";
    } else if (grouped) {
        result.format = decimals > 0 ? "#,##0.00" : "#,##0";
    } else if (parenthesized) {
        result.format = "0_);(0)";
    }
    return result;
}

// Recognize a simple or mixed fraction.
std::optional<number_match> match_fraction(std::string_view text) {
    cursor in(text);
    const bool negative = in.take('-');
    if (!negative)
        in.take('+');
    auto first = in.integer();
    if (!first)
        return std::nullopt;
    double whole = 0;
    if (in.peek() == ' ') {
        in.spaces();
        whole = *first;
        first = in.integer();
        if (!first)
            return std::nullopt;
    }
    if (!in.take('/'))
        return std::nullopt;
    const auto bottom = in.integer();
    if (!bottom || *bottom == 0 || !in.done())
        return std::nullopt;
    // Without a whole part only a value 1 or less reads as a fraction:
    // 12/25 is a date.
    if (whole == 0)
        return std::nullopt;
    const double value = whole + static_cast<double>(*first) / *bottom;
    return number_match{negative ? -value : value, "# ?/?"};
}

} // namespace

std::optional<number_match> match_number(std::string_view text) {
    text = util::trim(text);
    if (text.empty())
        return std::nullopt;
    if (auto result = match_decimal(text))
        return result;
    if (auto result = match_fraction(text))
        return result;
    return match_temporal(text);
}

} // namespace numfmt
