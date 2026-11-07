//
// Recognizes what the user typed: numbers with grouping, percentages,
// currency, fractions, dates and times. A match carries the format the
// entry implies, so typing 12/25/2024 shows a date.
//
// Derived from Gnumeric src/number-match.c,
// Copyright (C) 1999 Miguel de Icaza, 2006 Morten Welinder.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#ifndef NUMFMT_MATCH_H
#define NUMFMT_MATCH_H

#include <optional>
#include <string>
#include <string_view>

namespace numfmt {

// A recognized number.
struct number_match {
    double value = 0;

    // The format the entry implies; empty when it implies none.
    std::string format;
};

//
// Recognize an entry as a number.
//
// Parameters:
//      text        - What was typed, without a leading '=' or '.
//
// Returns:
//      The number, or nothing when the text is not numeric.
//
// Sample call:
//      numfmt::match_number("$1,234.50")  // 1234.5, "$#,##0.00"
//
std::optional<number_match> match_number(std::string_view text);

} // namespace numfmt

#endif
