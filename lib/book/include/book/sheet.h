//
// One sheet: its cells, styles, column and row sizes, merges and the
// settings saved with it. Cells live in a node-based map per column, so
// a cell's address is stable while the dependency graph holds it, and
// iterating a range touches only cells that exist.
//
// Derived from Gnumeric src/sheet.c,
// Copyright (C) 2000-2007 Jody Goldberg, 1998-2001 Miguel de Icaza,
// Copyright (C) 2000-2018 Morten Welinder.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#ifndef BOOK_SHEET_H
#define BOOK_SHEET_H

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "book/axis.h"
#include "book/cell.h"
#include "book/style_map.h"
#include "numfmt/format.h"

namespace book {

class workbook;

// What a typed entry means, before it is stored.
struct parsed_entry {
    // The constant, when the entry is not a formula.
    value constant;

    // The formula, when it is one.
    expr::node_ptr formula;

    // The format implied by the entry, or empty.
    std::string format;

    // Why a formula failed to parse, or empty.
    std::string error;
};

// One selection saved with the sheet.
struct saved_selection {
    cell_pos cursor;
    std::vector<cell_range> ranges;
    cell_pos top_left;
};

// One sheet of a workbook.
class sheet final : public val::cell_source {
public:
    // Create an empty sheet.
    sheet(workbook &book, std::string name, int cols = val::default_cols,
          int rows = val::default_rows);

    // Remove this sheet's formulas from the dependency graph.
    ~sheet() override;

    sheet(const sheet &) = delete;
    sheet &operator=(const sheet &) = delete;

    const value &value_at(cell_pos pos) override;
    void for_each_cell(
        const cell_range &range,
        const std::function<void(cell_pos, const value &)> &visit) override;
    const std::string &source_name() const override { return name_; }
    bool is_formula(cell_pos pos) override;
    std::optional<std::string> formula_text(cell_pos pos) override;
    int col_count() const override { return cols_; }
    int row_count() const override { return rows_; }

    workbook &book() const { return *book_; }
    const std::string &name() const { return name_; }

    // Rename the sheet; the caller checks for duplicates.
    void set_name(std::string name) { name_ = std::move(name); }

    // Find a cell; null when none exists.
    cell *find(cell_pos pos);
    const cell *find(cell_pos pos) const;

    // Return a cell, creating it when missing.
    cell &fetch(cell_pos pos);

    // Store a constant, replacing what the cell held.
    void set_constant(cell_pos pos, value constant,
                      std::string value_format = {});

    // Store a formula, replacing what the cell held.
    void set_formula(cell_pos pos, expr::node_ptr formula);

    // Store an array formula over a rectangle.
    void set_array(const cell_range &range, expr::node_ptr formula);

    // Remove a cell's content; styles stay.
    void clear_cell(cell_pos pos);

    // Remove the content of every cell in a rectangle.
    void clear_range(const cell_range &range);

    //
    // Interpret what the user typed at a position.
    //
    // Parameters:
    //      pos     - The cell.
    //      text    - The entry, whole.
    //
    // Notes:
    //      A leading ' forces text. A cell formatted as text keeps the
    //      entry as text. Formulas start with =, or with + or - when
    //      the rest does not read as a number.
    //
    parsed_entry parse_entry(cell_pos pos, std::string_view text);

    // Return what an editor shows for a cell.
    std::string entry_text(cell_pos pos);

    // Return a cell's style.
    const style_ptr &style_at(cell_pos pos) const { return styles_.at(pos); }

    // Return the style map.
    style_map &styles() { return styles_; }
    const style_map &styles() const { return styles_; }

    // Apply a change to the styles of a rectangle.
    void apply_style(const cell_range &range, const style_patch &patch);

    axis &cols() { return col_axis_; }
    const axis &cols() const { return col_axis_; }
    axis &rows() { return row_axis_; }
    const axis &rows() const { return row_axis_; }

    // Return the merged regions.
    const std::vector<cell_range> &merged() const { return merged_; }

    // Merge a rectangle.
    void merge(const cell_range &range);

    // Remove every merge overlapping a rectangle.
    void unmerge(const cell_range &range);

    // Return the merge holding a cell, or null.
    const cell_range *merged_at(cell_pos pos) const;

    //
    // Return the used rectangle.
    //
    // Returns:
    //      The bounds of every cell with content, or nothing on an
    //      empty sheet.
    //
    std::optional<cell_range> used_range() const;

    // Visit formula cells.
    void for_each_formula(const std::function<void(cell &)> &visit);

    // Visit every existing cell in column then row order.
    void for_each_stored(const std::function<void(cell &)> &visit);

    // Mark a cell's dirty state for the calculator.
    void set_dirty_mark(cell_pos pos, bool dirty);

    //
    // Find a dirty cell in a rectangle.
    //
    // Parameters:
    //      range   - Where to look.
    //
    // Returns:
    //      A dirty cell not being calculated, or null.
    //
    cell *first_dirty(const cell_range &range);

    // Visit dirty cells in a rectangle, calculating or not.
    void for_each_dirty(const cell_range &range,
                        const std::function<void(cell &)> &visit);

    // Report whether any cell is dirty.
    bool has_dirty() const { return dirty_count_ > 0; }

    // Move rows of cells, styles, sizes and merges; see workbook.
    void shift_rows_storage(int at, int count);
    void shift_cols_storage(int at, int count);

    // Move stored cells by a mapping, keeping their identity.
    void remap_cells(const std::function<std::optional<cell_pos>(cell_pos)> &map);

    // Saved settings of the sheet.
    bool show_grid = true;
    bool show_formulas = false;
    bool show_zeros = true;
    bool show_col_header = true;
    bool show_row_header = true;
    bool hidden = false;
    bool protect = false;
    double zoom = 1.0;
    std::optional<util::color> tab_color;
    saved_selection selection;

    // Parts of the file this build does not model, saved back verbatim.
    std::string preserved_xml;

private:
    friend class workbook;

    // One column: cells by row.
    using column = std::map<int, cell>;

    // Drop a formula's edges and marks before changing a cell.
    void forget_formula(cell &item);

    // Finish storing into a cell.
    void content_changed(cell &item);

    workbook *book_;
    std::string name_;
    int cols_;
    int rows_;
    std::map<int, column> columns_;
    style_map styles_;
    axis col_axis_;
    axis row_axis_;
    std::vector<cell_range> merged_;
    std::map<int, std::set<int>> dirty_rows_;
    std::size_t dirty_count_ = 0;
};

} // namespace book

#endif
