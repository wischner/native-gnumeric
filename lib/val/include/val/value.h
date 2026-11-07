//
// The value of a cell or of any subexpression. Gnumeric's tagged union
// of allocated structs becomes a std::variant with value semantics;
// strings and arrays are shared and immutable, so copying is cheap.
//
// The engine above this library reaches cells through the abstract
// cell_source, which keeps values free of any dependency on sheets.
//
// Derived from Gnumeric src/value.c,
// Copyright (C) 1998-2009 Miguel de Icaza, Michael Meeks, Jody Goldberg,
// Morten Welinder.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#ifndef VAL_VALUE_H
#define VAL_VALUE_H

#include <compare>
#include <concepts>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "val/coords.h"

namespace val {

// The standard spreadsheet errors, in Gnumeric's order.
enum class error_code : std::uint8_t {
    null,
    div0,
    value,
    ref,
    name,
    num,
    na,
    getting_data,
    spill,
    unknown
};

// Return an error's display text, such as "#DIV/0!".
std::string_view error_name(error_code code);

// Return the error spelled by text; nothing when it is not one.
std::optional<error_code> parse_error(std::string_view text);

class value;
class array_value;

// Gives values access to the cells of a sheet.
class cell_source {
public:
    virtual ~cell_source() = default;

    // Return a cell's value, calculating it first when it is stale.
    virtual const value &value_at(cell_pos pos) = 0;

    //
    // Visit the cells that exist inside a range.
    //
    // Parameters:
    //      range       - Cells of interest.
    //      visit       - Called in column then row order.
    //
    // Notes:
    //      Empty cells are not visited, so aggregating a whole column
    //      costs only what the column holds.
    //
    virtual void for_each_cell(
        const cell_range &range,
        const std::function<void(cell_pos, const value &)> &visit) = 0;

    // Return the sheet's name.
    virtual const std::string &source_name() const = 0;

    // Report whether a cell holds a formula.
    virtual bool is_formula(cell_pos pos) {
        (void)pos;
        return false;
    }

    // Return a cell's formula text, or nothing when it holds none.
    virtual std::optional<std::string> formula_text(cell_pos pos) {
        (void)pos;
        return std::nullopt;
    }

    // Return the number of columns.
    virtual int col_count() const = 0;

    // Return the number of rows.
    virtual int row_count() const = 0;
};

// A rectangle of cells on a sheet, as a value.
struct area {
    cell_source *sheet = nullptr;
    cell_range range;

    friend bool operator==(const area &, const area &) = default;
};

// How two values compare; arrays and ranges do not.
enum class comparison { less, equal, greater, mismatch };

// One scalar, area or array.
class value {
public:
    // Construct an empty value.
    value() = default;

    // Construct a number.
    value(double number) : data_(number) {}

    // Construct a number from an integer.
    value(int number) : data_(static_cast<double>(number)) {}

    // Construct a boolean; the overload is deliberately exact.
    template <std::same_as<bool> T> value(T flag) : data_(flag) {}

    // Construct an error.
    value(error_code code) : data_(code) {}

    // Construct a string.
    value(std::string_view text)
        : data_(std::make_shared<const std::string>(text)) {}

    // Construct a string.
    value(const char *text) : value(std::string_view(text)) {}

    // Construct a string.
    value(const std::string &text) : value(std::string_view(text)) {}

    // Construct an area.
    value(area cells) : data_(cells) {}

    // Construct an array.
    value(std::shared_ptr<const array_value> elements)
        : data_(std::move(elements)) {}

    bool is_empty() const {
        return std::holds_alternative<std::monostate>(data_);
    }
    bool is_bool() const { return std::holds_alternative<bool>(data_); }
    bool is_number() const { return std::holds_alternative<double>(data_); }
    bool is_error() const {
        return std::holds_alternative<error_code>(data_);
    }
    bool is_string() const {
        return std::holds_alternative<string_ptr>(data_);
    }
    bool is_area() const { return std::holds_alternative<area>(data_); }
    bool is_array() const { return std::holds_alternative<array_ptr>(data_); }

    // Report whether this is an area or an array.
    bool is_grid() const { return is_area() || is_array(); }

    // Report whether this is a number or a boolean.
    bool is_numeric() const { return is_number() || is_bool(); }

    // Return the number held; undefined for any other type.
    double number() const { return std::get<double>(data_); }

    // Return the flag held; undefined for any other type.
    bool flag() const { return std::get<bool>(data_); }

    // Return the error held; undefined for any other type.
    error_code error() const { return std::get<error_code>(data_); }

    // Return the string held; undefined for any other type.
    const std::string &string() const { return *std::get<string_ptr>(data_); }

    // Return the area held; undefined for any other type.
    const area &cells() const { return std::get<area>(data_); }

    // Return the array held; undefined for any other type.
    const array_value &elements() const { return *std::get<array_ptr>(data_); }

    //
    // Convert to a number.
    //
    // Returns:
    //      Numbers as they are, booleans as 0 or 1, empty as 0, strings
    //      that read as numbers; nothing for other strings and types.
    //
    std::optional<double> to_number() const;

    // Convert to a boolean; nothing when it does not read as one.
    std::optional<bool> to_bool() const;

    // Convert to text the way the & operator does.
    std::string to_text() const;

    // Return the number of columns of a grid, or 1.
    int grid_cols() const;

    // Return the number of rows of a grid, or 1.
    int grid_rows() const;

    // Return one element of a grid; a scalar returns itself.
    value grid_at(int col, int row) const;

    //
    // Compare two values in sort order.
    //
    // Parameters:
    //      other           - Right operand.
    //      case_sensitive  - Whether strings keep case.
    //
    // Notes:
    //      Numbers sort before strings before booleans before errors.
    //      Empty equals zero, FALSE and the empty string.
    //
    comparison compare(const value &other,
                       bool case_sensitive = false) const;

    // Report whether two values are identical, type included.
    friend bool operator==(const value &a, const value &b);

private:
    using string_ptr = std::shared_ptr<const std::string>;
    using array_ptr = std::shared_ptr<const array_value>;

    std::variant<std::monostate, bool, double, error_code, string_ptr, area,
                 array_ptr>
        data_;
};

// An immutable rectangle of values.
class array_value {
public:
    // Create an array from row-major elements.
    array_value(int cols, int rows, std::vector<value> elements);

    // Create an array filled with empty values.
    array_value(int cols, int rows);

    int cols() const { return cols_; }
    int rows() const { return rows_; }

    // Return one element.
    const value &at(int col, int row) const {
        return elements_[static_cast<std::size_t>(row) * cols_ + col];
    }

    // Replace one element while the array is being built.
    void set(int col, int row, value element) {
        elements_[static_cast<std::size_t>(row) * cols_ + col] =
            std::move(element);
    }

private:
    int cols_;
    int rows_;
    std::vector<value> elements_;
};

// Parse "TRUE" or "FALSE" in any case; nothing for anything else.
std::optional<bool> parse_boolean(std::string_view text);

} // namespace val

#endif
