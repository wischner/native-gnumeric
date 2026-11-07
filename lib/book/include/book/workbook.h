//
// A workbook owns its sheets and is the calculation engine: it answers
// the names formulas mention, tracks what reads what, and recalculates.
//
// Calculation walks an explicit stack, computing a formula's stale
// inputs before the formula. Gnumeric recurses and guards the depth;
// a chain as long as a sheet needs neither here. A cycle is detected by
// meeting a cell already on the stack and is resolved by iteration
// under the workbook's settings, as Gnumeric's protocol does.
//
// Derived from Gnumeric src/workbook.c and src/dependent.c,
// Copyright (C) 2000-2006 Jody Goldberg, 1998-2002 Michael Meeks,
// 2000-2018 Morten Welinder.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#ifndef BOOK_WORKBOOK_H
#define BOOK_WORKBOOK_H

#include <map>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "book/dependents.h"
#include "book/sheet.h"
#include "expr/function.h"

namespace book {

// One saved formula during a structural edit, for undo.
struct formula_change {
    sheet *where = nullptr;
    cell_pos pos;
    expr::node_ptr old_formula;
};

// One saved named definition during a structural edit, for undo.
struct name_change {
    std::shared_ptr<expr::named_expr> name;
    expr::node_ptr old_definition;
};

// What undoing a structural edit needs back.
struct rewrite_record {
    std::vector<formula_change> formulas;
    std::vector<name_change> names;
};

// A sheet taken out of a workbook, with what putting it back needs.
struct removed_sheet {
    std::unique_ptr<sheet> item;
    rewrite_record record;
    std::vector<std::shared_ptr<expr::named_expr>> names;
};

// A set of sheets and their calculation.
class workbook final : public expr::name_scope {
public:
    // Create a workbook with no sheets.
    workbook();

    ~workbook() override;

    workbook(const workbook &) = delete;
    workbook &operator=(const workbook &) = delete;

    expr::cell_source *find_sheet(std::string_view name) override;
    const expr::function_def *find_function(std::string_view name) override;
    std::shared_ptr<expr::named_expr>
    find_name(std::string_view name, expr::cell_source *from,
              bool create) override;

    // Append a sheet.
    sheet &add_sheet(std::string name, int cols = val::default_cols,
                     int rows = val::default_rows);

    // Insert a sheet at an index.
    sheet &insert_sheet(int index, std::string name,
                        int cols = val::default_cols,
                        int rows = val::default_rows);

    //
    // Delete a sheet.
    //
    // Notes:
    //      References to it become #REF!. The last sheet cannot go.
    //
    // Returns:
    //      Whether the sheet was deleted.
    //
    bool remove_sheet(int index);

    //
    // Take a sheet out, keeping it alive.
    //
    // Notes:
    //      Formulas elsewhere hold pointers to a sheet, so undo must put
    //      the same object back, not a copy. The workbook must outlive
    //      what this returns.
    //
    removed_sheet take_sheet(int index);

    // Put a sheet back where it was, with the formulas that read it.
    void restore_sheet(int index, removed_sheet removed);

    // Move a sheet to a new index.
    void move_sheet(int from, int to);

    // Return the number of sheets.
    int sheet_count() const { return static_cast<int>(sheets_.size()); }

    // Return a sheet by index.
    sheet &sheet_at(int index) { return *sheets_[static_cast<std::size_t>(index)]; }
    const sheet &sheet_at(int index) const {
        return *sheets_[static_cast<std::size_t>(index)];
    }

    // Return a sheet's index, or -1.
    int index_of(const sheet &item) const;

    // Return a sheet name not yet in use, Sheet1, Sheet2...
    std::string unique_sheet_name() const;

    // Return the shared styles.
    style_table &styles() { return styles_; }

    // Return the defined names, in definition order of their keys.
    const std::vector<std::shared_ptr<expr::named_expr>> &names() const {
        return names_;
    }

    // Define or redefine a name.
    std::shared_ptr<expr::named_expr> define_name(std::string_view name,
                                                  expr::node_ptr definition,
                                                  sheet *scope,
                                                  cell_pos origin);

    // Remove a name's definition.
    void undefine_name(const std::shared_ptr<expr::named_expr> &name);

    // Install a formula's reads in the graph.
    void attach(cell &formula);

    // Remove a formula's reads from the graph.
    void detach(cell &formula);

    // Mark what reads a cell as stale, transitively.
    void queue_readers(sheet &where, cell_pos pos);

    // Mark one formula and what reads it as stale.
    void queue_cell(cell &item);

    // Mark every formula stale.
    void queue_all();

    // Calculate everything stale, volatile formulas included.
    void recalculate();

    // Bring one cell up to date; used when a formula reads a stale cell.
    void calculate(cell &item);

    // Set whether cycles are iterated.
    bool iteration_enabled = true;
    int iteration_limit = 100;
    double iteration_tolerance = 0.001;

    // Set whether edits recalculate at once.
    bool automatic_recalculation = true;

    //
    // Rewrite every formula and name after a structural edit.
    //
    // Parameters:
    //      mapper  - The edit.
    //      skip    - A sheet to leave alone.
    //
    // Returns:
    //      What undo restores.
    //
    rewrite_record rewrite_formulas(expr::ref_mapper &mapper,
                                    const sheet *skip = nullptr);

    // Put formulas and names back from a record.
    void restore_formulas(const rewrite_record &record);

    // Rebuild the dependency graph from the sheets.
    void rebuild_dependencies();

    // Insert rows; references follow. Returns what undo needs.
    rewrite_record insert_rows(sheet &where, int at, int count);

    // Delete rows; references follow. Returns what undo needs.
    rewrite_record delete_rows(sheet &where, int at, int count);

    // Insert columns; references follow. Returns what undo needs.
    rewrite_record insert_cols(sheet &where, int at, int count);

    // Delete columns; references follow. Returns what undo needs.
    rewrite_record delete_cols(sheet &where, int at, int count);

    // Move a rectangle to another place, references following it.
    rewrite_record move_range(sheet &where, const cell_range &source,
                              sheet &target, cell_pos corner);

    // Attributes of the file, saved back.
    std::map<std::string, std::string> attributes;

    // Parts of the file this build does not model, saved back verbatim.
    std::string preserved_xml;

private:
    friend class sheet;

    // Evaluate one cell whose inputs are ready.
    void evaluate(cell &item);

    // Evaluate an array corner and spread its result.
    void evaluate_array(cell &item);

    // Iterate cycle members until they settle.
    void iterate_cycle();

    std::vector<std::unique_ptr<sheet>> sheets_;
    style_table styles_;
    const expr::function_table *functions_;
    std::vector<std::shared_ptr<expr::named_expr>> names_;
    dependents dependents_;
    std::set<cell *> volatile_;
    std::vector<cell *> cycle_;
    bool in_top_level_ = false;
};

} // namespace book

#endif
