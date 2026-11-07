//
// One cell. Gnumeric's GnmCell, with its dependent base flattened in:
// here the dependency graph holds cells directly.
//
// Derived from Gnumeric src/cell.c,
// Copyright (C) 1998-2002 Miguel de Icaza, Jody Goldberg, Morten Welinder.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#ifndef BOOK_CELL_H
#define BOOK_CELL_H

#include <utility>
#include <vector>

#include "expr/node.h"
#include "val/coords.h"
#include "val/value.h"

namespace book {

class sheet;

using val::cell_pos;
using val::cell_range;
using val::value;

// One rectangle a formula reads.
struct precedent {
    val::cell_source *sheet = nullptr;
    cell_range range;
};

// The part of an array formula a cell plays.
struct array_part {
    // The corner cell of the array.
    cell_pos corner;

    // The array's size.
    int cols = 1;
    int rows = 1;
};

// One stored cell.
class cell {
public:
    // Create an empty cell.
    cell(sheet &owner, cell_pos pos) : owner_(&owner), pos_(pos) {}

    sheet &owner() const { return *owner_; }
    cell_pos pos() const { return pos_; }

    // Update the position after rows or columns move.
    void set_pos(cell_pos pos) { pos_ = pos; }

    // Report whether the cell holds a formula, array parts included.
    bool has_formula() const { return formula != nullptr || array_member(); }

    // Report whether the cell belongs to an array formula.
    bool array_member() const { return array.cols > 1 || array.rows > 1 ||
                                       in_array; }

    // The formula tree; null for constants and array elements.
    expr::node_ptr formula;

    // The constant, or the most recent result of the formula.
    value result;

    // Set when this cell is part of an array formula.
    bool in_array = false;
    array_part array;

    // The format a constant arrived with, in Gnumeric's ValueFormat.
    std::string value_format;

    // What the formula reads; a copy of the dependency graph's edges.
    std::vector<precedent> reads;

    // The calculation state.
    bool dirty = false;
    bool being_calculated = false;
    bool in_cycle = false;
    bool is_volatile = false;

private:
    sheet *owner_;
    cell_pos pos_;
};

} // namespace book

#endif
