//
// The private parsed form of a number format, shared by the parser and
// the renderer.
//
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#ifndef NUMFMT_FORMAT_SECTION_H
#define NUMFMT_FORMAT_SECTION_H

#include <optional>
#include <string>
#include <vector>

#include "numfmt/format.h"

namespace numfmt {

// One element of a format section.
enum class token_kind {
    literal,        // Text copied as is.
    digit_zero,     // 0: always a digit.
    digit_hash,     // #: a digit when significant.
    digit_blank,    // ?: a digit or a space.
    decimal,        // . between integer and fraction digits.
    exponent,       // E+ or E-.
    fraction_bar,   // / between numerator and denominator.
    text_body,      // @: the text value.
    year_2, year_4,
    month_1, month_2, month_abbrev, month_name, month_letter,
    day_1, day_2, day_abbrev, day_name,
    hour_1, hour_2,
    minute_1, minute_2,
    second_1, second_2,
    elapsed_hours, elapsed_minutes, elapsed_seconds,
    second_fraction,
    am_pm,
};

// One element and its payload.
struct format_token {
    token_kind kind = token_kind::literal;
    std::string text;       // Literal text, or the AM/PM spelling.
    int size = 0;           // Digits in a second_fraction.
};

// A numeric comparison selecting a section.
struct section_condition {
    enum class op { lt, le, gt, ge, eq, ne } comparison = op::ge;
    double limit = 0;

    // Report whether a value satisfies the condition.
    bool matches(double value) const;
};

// One semicolon-separated part of a format.
struct format_section {
    std::vector<format_token> tokens;
    std::optional<util::color> color;
    std::optional<section_condition> condition;
    bool general = false;           // Uses the word General.
    bool temporal = false;          // Holds date or time tokens.
    bool has_time = false;          // Holds hour, minute or second tokens.
    bool has_date = false;          // Holds year, month or day tokens.
    bool has_digits = false;        // Holds digit placeholders.
    bool has_text = false;          // Holds @.
    bool thousands = false;         // A comma between integer digits.
    bool fraction = false;          // Holds a fraction bar.
    bool has_currency = false;      // Holds a currency symbol.
    int scale_commas = 0;           // Trailing commas: divide by 1000.
    int percents = 0;               // Percent signs: multiply by 100.
    int integer_digits = 0;         // Placeholders before the point.
    int decimal_digits = 0;         // Placeholders after the point.
    int exponent_digits = 0;        // Placeholders after E.
    bool exponent_plus = false;     // E+ forces a sign.
    int numerator_digits = 0;       // Placeholders before the bar.
    int denominator_digits = 0;     // Placeholders after the bar.
    int fixed_denominator = 0;      // A literal denominator such as /16.
};

// Parse a format into sections.
std::vector<format_section> parse_sections(std::string_view source);

// Render a magnitude through one numeric section.
std::string render_numeric(const format_section &section, double value);

// Render a serial through one temporal section.
std::string render_temporal(const format_section &section, double value);

} // namespace numfmt

#endif
