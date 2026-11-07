//
// A rectangle of cells lifted off a sheet: the clipboard's content and
// the memento behind undo. Formulas stay as relative trees, so pasting
// one somewhere else is pasting the same tree at another origin.
//
// Derived from Gnumeric src/clipboard.c,
// Copyright (C) 1999-2008 Miguel de Icaza, Jody Goldberg, Morten Welinder.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#ifndef BOOK_REGION_H
#define BOOK_REGION_H

#include <optional>
#include <string>
#include <vector>

#include "book/sheet.h"

namespace book {

// One lifted cell.
struct region_cell {
    int col = 0;            // Offset from the region's corner.
    int row = 0;
    value constant;
    std::string value_format;
    expr::node_ptr formula;

    // Set on an array corner: the array's size.
    int array_cols = 0;
    int array_rows = 0;
};

// A run of one style inside a region.
struct region_style {
    cell_range range;       // Relative to the region's corner.
    style_ptr item;
};

// A lifted rectangle.
struct region {
    int cols = 0;
    int rows = 0;
    std::vector<region_cell> cells;
    std::vector<region_style> styles;
    std::vector<cell_range> merges;     // Relative to the corner.

    // Column and row sizes, present when whole lines were lifted.
    std::vector<double> col_points;
    std::vector<double> row_points;
};

// What to bring along when pasting.
struct paste_options {
    bool contents = true;
    bool formats = true;

    // Replace formulas with their results.
    bool values_only = false;

    // Swap rows and columns.
    bool transpose = false;
};

// Lift a rectangle off a sheet.
region copy_region(sheet &from, const cell_range &range);

//
// Put a lifted rectangle down.
//
// Parameters:
//      into    - Target sheet.
//      corner  - Top-left cell.
//      item    - What to place.
//      options - What to bring along.
//
// Notes:
//      A one-cell region fills a larger target through
//      paste_repeated. Cells past the edge of the sheet are dropped.
//
void paste_region(sheet &into, cell_pos corner, const region &item,
                  const paste_options &options = {});

// Repeat a region over a larger rectangle.
void paste_repeated(sheet &into, const cell_range &target, const region &item,
                    const paste_options &options = {});

// Render a region as tab-separated text for the system clipboard.
std::string region_as_text(sheet &from, const cell_range &range);

// Return the text a cell shows.
std::string display_text(sheet &from, cell_pos pos, int max_chars = -1);

// Store tab-separated text, one entry per cell.
void paste_text(sheet &into, cell_pos corner, std::string_view text);

// Store an entry into a cell, applying the format it implies.
void store_entry(sheet &into, cell_pos pos, std::string_view text);

} // namespace book

#endif
