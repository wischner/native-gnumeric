//
// Cell positions and rectangular ranges, and their A1 spelling.
//
// Derived from Gnumeric src/position.c, src/ranges.c and parse-util.c,
// Copyright (C) 1998 Miguel de Icaza, 2001 Jody Goldberg, and contributors.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#ifndef VAL_COORDS_H
#define VAL_COORDS_H

#include <algorithm>
#include <compare>
#include <optional>
#include <string>
#include <string_view>

namespace val {

// Gnumeric's default sheet size.
inline constexpr int default_cols = 256;
inline constexpr int default_rows = 65536;

// A zero-based column and row.
struct cell_pos {
    int col = 0;
    int row = 0;

    friend auto operator<=>(const cell_pos &, const cell_pos &) = default;
};

// An inclusive rectangle of cells.
struct cell_range {
    cell_pos start;
    cell_pos end;

    friend auto operator<=>(const cell_range &, const cell_range &) = default;

    // Return the range covering one cell.
    static cell_range single(cell_pos pos) { return {pos, pos}; }

    // Return the range between two corners in any order.
    static cell_range between(cell_pos a, cell_pos b) {
        return {{std::min(a.col, b.col), std::min(a.row, b.row)},
                {std::max(a.col, b.col), std::max(a.row, b.row)}};
    }

    // Return the number of columns.
    int cols() const { return end.col - start.col + 1; }

    // Return the number of rows.
    int rows() const { return end.row - start.row + 1; }

    // Report whether a position lies inside.
    bool contains(cell_pos pos) const {
        return pos.col >= start.col && pos.col <= end.col &&
               pos.row >= start.row && pos.row <= end.row;
    }

    // Report whether another range lies wholly inside.
    bool contains(const cell_range &other) const {
        return contains(other.start) && contains(other.end);
    }

    // Report whether two ranges share a cell.
    bool overlaps(const cell_range &other) const {
        return start.col <= other.end.col && other.start.col <= end.col &&
               start.row <= other.end.row && other.start.row <= end.row;
    }

    // Return the shared cells of two overlapping ranges.
    cell_range intersect(const cell_range &other) const {
        return {{std::max(start.col, other.start.col),
                 std::max(start.row, other.start.row)},
                {std::min(end.col, other.end.col),
                 std::min(end.row, other.end.row)}};
    }

    // Report whether this is one cell.
    bool is_single() const { return start == end; }
};

// Return a column's letters, such as "AB" for 27.
std::string col_name(int col);

// Return a row's one-based number as text.
std::string row_name(int row);

// Return a cell's name, such as "B3".
std::string cell_name(cell_pos pos);

// Return a range's name, one cell naming itself.
std::string range_name(const cell_range &range);

// Parse column letters; nothing when they are not letters or too long.
std::optional<int> parse_col(std::string_view letters);

// Parse "B3" or "$B$3"; nothing when malformed.
std::optional<cell_pos> parse_cell_name(std::string_view text);

// Parse "B3" or "B3:C4"; nothing when malformed.
std::optional<cell_range> parse_range_name(std::string_view text);

} // namespace val

#endif
