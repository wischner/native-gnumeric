//
// Color parsing and formatting.
//
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include "util/color.h"

#include <array>
#include <charconv>
#include <cstdio>

#include "util/text.h"

namespace util {
namespace {

// One name a number format or stylesheet may use.
struct named_color {
    std::string_view name;
    color value;
};

constexpr std::array<named_color, 10> names{{
    {"black", {0, 0, 0}},
    {"white", {255, 255, 255}},
    {"red", {255, 0, 0}},
    {"green", {0, 128, 0}},
    {"blue", {0, 0, 255}},
    {"yellow", {255, 255, 0}},
    {"magenta", {255, 0, 255}},
    {"cyan", {0, 255, 255}},
    {"gray", {128, 128, 128}},
    {"orange", {255, 165, 0}},
}};

// Parse one hexadecimal field to its top byte.
std::optional<std::uint8_t> channel(std::string_view text) {
    unsigned parsed = 0;
    const char *end = text.data() + text.size();
    const auto result = std::from_chars(text.data(), end, parsed, 16);
    if (result.ec != std::errc{} || result.ptr != end || parsed > 0xffff)
        return std::nullopt;
    return static_cast<std::uint8_t>(text.size() <= 2 ? parsed : parsed >> 8);
}

} // namespace

std::optional<color> parse_gnumeric_color(std::string_view text) {
    const auto first = text.find(':');
    if (first == std::string_view::npos)
        return std::nullopt;
    const auto second = text.find(':', first + 1);
    if (second == std::string_view::npos)
        return std::nullopt;
    const auto r = channel(text.substr(0, first));
    const auto g = channel(text.substr(first + 1, second - first - 1));
    const auto b = channel(text.substr(second + 1));
    if (!r || !g || !b)
        return std::nullopt;
    return color{*r, *g, *b};
}

std::string format_gnumeric_color(color value) {
    char buffer[32];
    std::snprintf(buffer, sizeof buffer, "%X:%X:%X", value.r * 0x101u,
                  value.g * 0x101u, value.b * 0x101u);
    return buffer;
}

std::optional<color> parse_css_color(std::string_view text) {
    if (text.size() == 7 && text[0] == '#') {
        const auto r = channel(text.substr(1, 2));
        const auto g = channel(text.substr(3, 2));
        const auto b = channel(text.substr(5, 2));
        if (r && g && b)
            return color{*r, *g, *b};
        return std::nullopt;
    }
    const std::string lowered = to_lower(text);
    for (const named_color &entry : names)
        if (entry.name == lowered)
            return entry.value;
    return std::nullopt;
}

std::string format_css_color(color value) {
    char buffer[16];
    std::snprintf(buffer, sizeof buffer, "#%02x%02x%02x", value.r, value.g,
                  value.b);
    return buffer;
}

} // namespace util
