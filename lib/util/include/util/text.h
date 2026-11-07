//
// Text helpers shared by the parsers and functions: Unicode case
// mapping, comparison without case, and trimming. The strings are
// UTF-8 throughout.
//
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#ifndef UTIL_TEXT_H
#define UTIL_TEXT_H

#include <compare>
#include <string>
#include <string_view>

namespace util {

// Return text in lower case, covering Latin, Greek and Cyrillic.
std::string to_lower(std::string_view text);

// Return text in upper case, covering Latin, Greek and Cyrillic.
std::string to_upper(std::string_view text);

// Report whether two strings are equal ignoring case.
bool equal_nocase(std::string_view a, std::string_view b);

// Order two strings ignoring case.
std::strong_ordering compare_nocase(std::string_view a, std::string_view b);

// Return text without leading and trailing ASCII white space.
std::string_view trim(std::string_view text);

// Return the number of characters in UTF-8 text.
std::size_t char_count(std::string_view utf8);

// Return the byte offset of a character index, clamped to the end.
std::size_t byte_offset(std::string_view utf8, std::size_t character);

// Format a double the shortest way that reads back identically.
std::string shortest_double(double value);

} // namespace util

#endif
