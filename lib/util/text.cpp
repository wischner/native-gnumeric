//
// Case mapping over UTF-32 and shortest round-trip number output. The
// double output follows Gnumeric's go_dtoa("!g"): plain notation for
// ordinary magnitudes, an exponent outside them.
//
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include "util/text.h"

#include <charconv>
#include <cmath>
#include <cstdio>

#include "util/utf8.h"

namespace util {
namespace {

// Map a code point to lower case.
char32_t lower_of(char32_t c) {
    if (c >= U'A' && c <= U'Z')
        return c + 32;
    if ((c >= 0xC0 && c <= 0xDE && c != 0xD7) ||
        (c >= 0x391 && c <= 0x3A9) || (c >= 0x410 && c <= 0x42F))
        return c + 32;
    if (c >= 0x400 && c <= 0x40F)
        return c + 80;
    if (c >= 0x100 && c <= 0x17F) {
        if ((c >= 0x139 && c <= 0x148) || (c >= 0x179 && c <= 0x17E))
            return (c & 1) ? c + 1 : c;
        if (c != 0x130 && c != 0x131 && c != 0x138 && c != 0x149 &&
            c != 0x178 && c != 0x17F)
            return (c & 1) ? c : c + 1;
    }
    return c;
}

// Map a code point to upper case.
char32_t upper_of(char32_t c) {
    if (c >= U'a' && c <= U'z')
        return c - 32;
    if ((c >= 0xE0 && c <= 0xFE && c != 0xF7) ||
        (c >= 0x3B1 && c <= 0x3C9 && c != 0x3C2) ||
        (c >= 0x430 && c <= 0x44F))
        return c - 32;
    if (c >= 0x450 && c <= 0x45F)
        return c - 80;
    if (c >= 0x100 && c <= 0x17F) {
        if ((c >= 0x139 && c <= 0x148) || (c >= 0x179 && c <= 0x17E))
            return (c & 1) ? c : c - 1;
        if (c != 0x130 && c != 0x131 && c != 0x138 && c != 0x149 &&
            c != 0x178 && c != 0x17F)
            return (c & 1) ? c - 1 : c;
    }
    return c;
}

// Report whether a string is plain ASCII.
bool is_ascii(std::string_view text) {
    for (const char c : text)
        if (static_cast<unsigned char>(c) >= 0x80)
            return false;
    return true;
}

} // namespace

std::string to_lower(std::string_view text) {
    if (is_ascii(text)) {
        std::string out(text);
        for (char &c : out)
            if (c >= 'A' && c <= 'Z')
                c = static_cast<char>(c + 32);
        return out;
    }
    std::u32string wide = to_utf32(text);
    for (char32_t &c : wide)
        c = lower_of(c);
    return to_utf8(wide);
}

std::string to_upper(std::string_view text) {
    if (is_ascii(text)) {
        std::string out(text);
        for (char &c : out)
            if (c >= 'a' && c <= 'z')
                c = static_cast<char>(c - 32);
        return out;
    }
    std::u32string wide = to_utf32(text);
    for (char32_t &c : wide)
        c = upper_of(c);
    return to_utf8(wide);
}

bool equal_nocase(std::string_view a, std::string_view b) {
    if (is_ascii(a) && is_ascii(b)) {
        if (a.size() != b.size())
            return false;
        for (std::size_t i = 0; i < a.size(); ++i) {
            char x = a[i];
            char y = b[i];
            if (x >= 'A' && x <= 'Z')
                x = static_cast<char>(x + 32);
            if (y >= 'A' && y <= 'Z')
                y = static_cast<char>(y + 32);
            if (x != y)
                return false;
        }
        return true;
    }
    return to_lower(a) == to_lower(b);
}

std::strong_ordering compare_nocase(std::string_view a, std::string_view b) {
    return to_lower(a).compare(to_lower(b)) <=> 0;
}

std::string_view trim(std::string_view text) {
    const auto space = [](char c) {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r';
    };
    while (!text.empty() && space(text.front()))
        text.remove_prefix(1);
    while (!text.empty() && space(text.back()))
        text.remove_suffix(1);
    return text;
}

std::size_t char_count(std::string_view utf8) {
    std::size_t count = 0;
    for (const char c : utf8)
        if ((static_cast<unsigned char>(c) & 0xC0) != 0x80)
            ++count;
    return count;
}

std::size_t byte_offset(std::string_view utf8, std::size_t character) {
    std::size_t seen = 0;
    for (std::size_t i = 0; i < utf8.size(); ++i) {
        if ((static_cast<unsigned char>(utf8[i]) & 0xC0) == 0x80)
            continue;
        if (seen == character)
            return i;
        ++seen;
    }
    return utf8.size();
}

std::string shortest_double(double value) {
    if (std::isnan(value))
        return "NaN";
    if (std::isinf(value))
        return value > 0 ? "Infinity" : "-Infinity";
    if (value == 0)
        return "0";
    char buffer[64];
    const double magnitude = std::fabs(value);
    const auto format = magnitude >= 1e-4 && magnitude < 1e15
                            ? std::chars_format::fixed
                            : std::chars_format::scientific;
    const auto result =
        std::to_chars(buffer, buffer + sizeof buffer, value, format);
    return std::string(buffer, result.ptr);
}

} // namespace util
