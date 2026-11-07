//
// Which style each cell of a sheet has. Per column, an interval map from
// the first row of a run to its style: Gnumeric's quad-tree of style
// tiles solves the same problem, but runs are far simpler and whole
// columns of one style stay one entry.
//
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#ifndef BOOK_STYLE_MAP_H
#define BOOK_STYLE_MAP_H

#include <functional>
#include <map>
#include <utility>
#include <vector>

#include "book/style.h"
#include "val/coords.h"

namespace book {

using val::cell_pos;
using val::cell_range;

// The style of every cell.
class style_map {
public:
    // Create a map of one style for a sheet size.
    style_map(int cols, int rows, style_ptr base);

    // Return a cell's style.
    const style_ptr &at(cell_pos pos) const;

    // Set a rectangle to one style.
    void set(const cell_range &range, const style_ptr &item);

    //
    // Rewrite the styles of a rectangle.
    //
    // Parameters:
    //      range   - Cells to change.
    //      change  - Maps one existing style to its replacement.
    //
    void transform(const cell_range &range,
                   const std::function<style_ptr(const style_ptr &)> &change);

    // Insert rows, copying the style above; negative counts delete.
    void shift_rows(int at, int count);

    // Insert columns, copying the column left; negative counts delete.
    void shift_cols(int at, int count);

    // Visit the runs of one column between two rows, clipped to them.
    void for_each_run(
        int col, int first, int last,
        const std::function<void(int, int, const style_ptr &)> &visit) const;

    //
    // Visit maximal rectangles of equal style that differ from a base.
    //
    // Notes:
    //      Adjacent columns with identical runs merge, so the saved
    //      form stays close to what Gnumeric writes.
    //
    void for_each_region(
        const style_ptr &skip,
        const std::function<void(const cell_range &, const style_ptr &)> &visit)
        const;

private:
    // One column: a run begins at each key.
    using column = std::map<int, style_ptr>;

    // Merge a run boundary with its neighbor when styles match.
    static void coalesce(column &runs, int row);

    int rows_;
    std::vector<column> columns_;
};

} // namespace book

#endif
