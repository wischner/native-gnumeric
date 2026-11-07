//
// Autofill, sorting and search over a sheet.
//
// Derived from Gnumeric src/sheet-autofill.c, src/sort.c and src/search.c,
// Copyright (C) 1998-2014 Miguel de Icaza, Jody Goldberg, Morten Welinder,
// JP Rosevear.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#ifndef BOOK_FILL_H
#define BOOK_FILL_H

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "book/sheet.h"

namespace book {

//
// Continue the cells of a source rectangle across a target.
//
// Parameters:
//      where   - The sheet.
//      source  - Cells whose pattern is continued.
//      target  - The source, grown in one direction.
//
// Notes:
//      Numbers continue their step, dates by month when they advance by
//      whole months, trailing numbers in text increment, day and month
//      names cycle, formulas are copied relative.
//
void autofill(sheet &where, const cell_range &source,
              const cell_range &target);

// One column to sort by.
struct sort_key {
    int offset = 0;     // Column offset, or row offset sorting columns.
    bool ascending = true;
    bool case_sensitive = false;
};

// Sort the rows of a rectangle by keys.
void sort_rows(sheet &where, const cell_range &range,
               const std::vector<sort_key> &keys);

// Sort the columns of a rectangle by keys.
void sort_cols(sheet &where, const cell_range &range,
               const std::vector<sort_key> &keys);

// What a search looks through.
struct search_options {
    bool match_case = false;
    bool whole_cell = false;
    bool in_formulas = true;
    bool in_values = true;
};

// One search hit.
struct search_hit {
    sheet *where = nullptr;
    cell_pos pos;
};

//
// Find text after a position in reading order.
//
// Parameters:
//      from    - Sheet to search.
//      after   - Cell after which to begin.
//      text    - What to find.
//      options - How to compare.
//      forward - Whether to walk forward.
//
// Returns:
//      The next hit, wrapping past the end; nothing when text is absent.
//
std::optional<search_hit> find_text(sheet &from, cell_pos after,
                                    std::string_view text,
                                    const search_options &options,
                                    bool forward = true);

//
// Replace text in a cell's entry.
//
// Returns:
//      Whether the cell changed.
//
bool replace_in_cell(sheet &where, cell_pos pos, std::string_view text,
                     std::string_view replacement,
                     const search_options &options);

} // namespace book

#endif
