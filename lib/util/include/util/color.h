//
// Display colors, and the conversions a workbook file needs: Gnumeric
// stores a channel as sixteen bits, written "RRRR:GGGG:BBBB" in hex.
//
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#ifndef UTIL_COLOR_H
#define UTIL_COLOR_H

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace util {

// A display color; alpha is not a document concept.
struct color {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;

    friend bool operator==(const color &, const color &) = default;
};

// Parse "RRRR:GGGG:BBBB" hexadecimal channels; nothing when malformed.
std::optional<color> parse_gnumeric_color(std::string_view text);

// Format a color as "RRRR:GGGG:BBBB", repeating each byte.
std::string format_gnumeric_color(color value);

// Return the color with an HTML name or "#rrggbb"; nothing when unknown.
std::optional<color> parse_css_color(std::string_view text);

// Format a color as lowercase "#rrggbb".
std::string format_css_color(color value);

} // namespace util

#endif
