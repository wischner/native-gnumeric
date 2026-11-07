//
// Number formats: the Excel format language Gnumeric stores in its
// Format attribute. A format is parsed once into sections of tokens and
// is immutable afterwards, so cells share one object per format string.
//
// Derived from goffice go-format.c (the format language) and Gnumeric
// src/gnm-format.c,
// Copyright (C) 2005-2015 Morten Welinder, Jody Goldberg and contributors.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#ifndef NUMFMT_FORMAT_H
#define NUMFMT_FORMAT_H

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "util/color.h"

namespace numfmt {

// What a format displays, for choosing alignment and offering dialogs.
enum class format_family {
    general,
    number,
    currency,
    percent,
    fraction,
    scientific,
    date,
    time,
    text,
    other
};

// The text of a value under a format.
struct rendered {
    std::string text;

    // Set when the section names a color, such as [Red].
    std::optional<util::color> color;
};

//
// Render a number the General way.
//
// Parameters:
//      value       - The number.
//      max_chars   - Width to fit, or negative for no limit.
//
// Returns:
//      Up to fifteen significant digits, fewer to fit max_chars, with
//      an exponent when the integer part cannot fit. A width too narrow
//      for any form still returns the shortest form.
//
std::string render_general(double value, int max_chars = -1);

// One section of a parsed format.
struct format_section;

// An immutable parsed format string.
class number_format {
public:
    //
    // Return the shared format for a string.
    //
    // Notes:
    //      Malformed format strings never fail: unknown letters become
    //      literal text, as Excel displays them.
    //
    static std::shared_ptr<const number_format> get(std::string_view source);

    // Return the shared General format.
    static std::shared_ptr<const number_format> general();

    ~number_format();

    // Return the source text.
    const std::string &source() const { return source_; }

    // Return what this format displays.
    format_family family() const { return family_; }

    // Report whether this is General.
    bool is_general() const { return family_ == format_family::general; }

    // Report whether numbers display as a date or a time.
    bool is_temporal() const {
        return family_ == format_family::date ||
               family_ == format_family::time;
    }

    //
    // Render a number.
    //
    // Parameters:
    //      value       - The number.
    //      max_chars   - Width for General sections, or negative.
    //
    rendered render(double value, int max_chars = -1) const;

    // Render text through the format's text section.
    rendered render_text(std::string_view text) const;

private:
    number_format(std::string source, std::vector<format_section> sections);

    std::string source_;
    std::vector<format_section> sections_;
    format_family family_;
};

} // namespace numfmt

#endif
