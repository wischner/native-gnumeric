//
// Value conversion and comparison.
//
// Derived from Gnumeric src/value.c,
// Copyright (C) 1998-2009 Miguel de Icaza, Michael Meeks, Jody Goldberg,
// Morten Welinder.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include "val/value.h"

#include <array>
#include <cmath>
#include <cstdio>

#include "numfmt/match.h"
#include "util/text.h"

namespace val {
namespace {

constexpr std::array<std::string_view, 10> error_names{
    "#NULL!", "#DIV/0!", "#VALUE!", "#REF!",  "#NAME?",
    "#NUM!",  "#N/A",    "#GETTING_DATA", "#SPILL!", "#UNKNOWN!"};

// Sort rank of a type: higher sorts later.
int type_rank(const value &item) {
    if (item.is_empty())
        return 0;
    if (item.is_number())
        return 1;
    if (item.is_string())
        return 2;
    if (item.is_bool())
        return 3;
    if (item.is_error())
        return 4;
    return 5;
}

// Convert an ordering to a comparison.
template <typename T> comparison order(const T &a, const T &b) {
    if (a < b)
        return comparison::less;
    if (b < a)
        return comparison::greater;
    return comparison::equal;
}

// Compare an empty value with a non-empty one.
comparison compare_with_empty(const value &other) {
    if (other.is_number())
        return order(0.0, other.number());
    if (other.is_bool())
        return order(false, other.flag());
    if (other.is_string())
        return other.string().empty() ? comparison::equal : comparison::less;
    if (other.is_error())
        return comparison::less;
    return comparison::mismatch;
}

} // namespace

std::string_view error_name(error_code code) {
    return error_names[static_cast<std::size_t>(code)];
}

std::optional<error_code> parse_error(std::string_view text) {
    for (std::size_t i = 0; i < error_names.size(); ++i)
        if (util::equal_nocase(error_names[i], text))
            return static_cast<error_code>(i);
    return std::nullopt;
}

std::optional<bool> parse_boolean(std::string_view text) {
    if (util::equal_nocase(text, "true"))
        return true;
    if (util::equal_nocase(text, "false"))
        return false;
    return std::nullopt;
}

std::optional<double> value::to_number() const {
    if (is_number())
        return number();
    if (is_bool())
        return flag() ? 1.0 : 0.0;
    if (is_empty())
        return 0.0;
    if (is_string()) {
        const auto match = numfmt::match_number(string());
        if (match)
            return match->value;
    }
    return std::nullopt;
}

std::optional<bool> value::to_bool() const {
    if (is_bool())
        return flag();
    if (is_number())
        return number() != 0;
    if (is_empty())
        return false;
    if (is_string())
        return parse_boolean(string());
    return std::nullopt;
}

std::string value::to_text() const {
    if (is_string())
        return string();
    if (is_number()) {
        // Gnumeric prints the shortest form that reads back identically.
        char buffer[64];
        std::snprintf(buffer, sizeof buffer, "%.15g", number());
        if (std::strtod(buffer, nullptr) != number())
            std::snprintf(buffer, sizeof buffer, "%.17g", number());
        return buffer;
    }
    if (is_bool())
        return flag() ? "TRUE" : "FALSE";
    if (is_error())
        return std::string(error_name(error()));
    if (is_area())
        return range_name(cells().range);
    return {};
}

int value::grid_cols() const {
    if (is_area())
        return cells().range.cols();
    if (is_array())
        return elements().cols();
    return 1;
}

int value::grid_rows() const {
    if (is_area())
        return cells().range.rows();
    if (is_array())
        return elements().rows();
    return 1;
}

value value::grid_at(int col, int row) const {
    if (is_area()) {
        const area &source = cells();
        return source.sheet->value_at(
            {source.range.start.col + col, source.range.start.row + row});
    }
    if (is_array())
        return elements().at(col, row);
    return *this;
}

comparison value::compare(const value &other, bool case_sensitive) const {
    if (is_grid() || other.is_grid())
        return comparison::mismatch;
    if (is_empty() && other.is_empty())
        return comparison::equal;
    if (is_empty())
        return compare_with_empty(other);
    if (other.is_empty()) {
        const comparison flipped = compare_with_empty(*this);
        if (flipped == comparison::less)
            return comparison::greater;
        if (flipped == comparison::greater)
            return comparison::less;
        return flipped;
    }
    const int left = type_rank(*this);
    const int right = type_rank(other);
    if (left != right)
        return order(left, right);
    if (is_number())
        return order(number(), other.number());
    if (is_bool())
        return order(flag(), other.flag());
    if (is_error())
        return order(static_cast<int>(error()),
                     static_cast<int>(other.error()));
    if (case_sensitive)
        return order(string(), other.string());
    const auto result = util::compare_nocase(string(), other.string());
    if (result < 0)
        return comparison::less;
    return result > 0 ? comparison::greater : comparison::equal;
}

bool operator==(const value &a, const value &b) {
    if (a.data_.index() != b.data_.index())
        return false;
    if (a.is_string())
        return a.string() == b.string();
    if (a.is_array()) {
        const array_value &left = a.elements();
        const array_value &right = b.elements();
        if (left.cols() != right.cols() || left.rows() != right.rows())
            return false;
        for (int row = 0; row < left.rows(); ++row)
            for (int col = 0; col < left.cols(); ++col)
                if (!(left.at(col, row) == right.at(col, row)))
                    return false;
        return true;
    }
    return a.data_ == b.data_;
}

array_value::array_value(int cols, int rows, std::vector<value> elements)
    : cols_(cols), rows_(rows), elements_(std::move(elements)) {}

array_value::array_value(int cols, int rows)
    : cols_(cols), rows_(rows),
      elements_(static_cast<std::size_t>(cols) * rows) {}

} // namespace val
