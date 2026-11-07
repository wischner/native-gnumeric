//
// The commands. Factories return ready command objects, so callers name
// what they want done and never see the memento classes.
//
// Derived from Gnumeric src/commands.c,
// Copyright (C) 1999-2008 Jody Goldberg, 2002-2009 Morten Welinder.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#ifndef CMD_EDITS_H
#define CMD_EDITS_H

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "book/fill.h"
#include "book/region.h"
#include "book/style.h"
#include "cmd/command.h"
#include "expr/node.h"

namespace cmd {

using book::sheet;
using val::cell_pos;
using val::cell_range;

// What Clear removes.
enum class clear_kind { contents, formats, all };

// Store an entry into a cell.
std::unique_ptr<command> set_entry(sheet &where, cell_pos pos,
                                   std::string text);

// Store one entry across a rectangle, relative formulas following.
std::unique_ptr<command> fill_entry(sheet &where, const cell_range &range,
                                    std::string text);

// Store an array formula over a rectangle.
std::unique_ptr<command> set_array(sheet &where, const cell_range &range,
                                   expr::node_ptr formula);

// Remove contents, formats or both from rectangles.
std::unique_ptr<command> clear_cells(sheet &where,
                                     std::vector<cell_range> ranges,
                                     clear_kind kind);

// Change the style of rectangles.
std::unique_ptr<command> format_cells(sheet &where,
                                      std::vector<cell_range> ranges,
                                      book::style_patch patch,
                                      std::string name);

// Paste a lifted region at a corner or across a target.
std::unique_ptr<command> paste(sheet &where, const cell_range &target,
                               book::region item, book::paste_options options);

// Paste plain text at a corner.
std::unique_ptr<command> paste_text(sheet &where, cell_pos corner,
                                    std::string text);

// Move a rectangle, as a cut followed by a paste.
std::unique_ptr<command> move_range(sheet &from, const cell_range &source,
                                    sheet &to, cell_pos corner);

// Continue a pattern across a target.
std::unique_ptr<command> autofill(sheet &where, const cell_range &source,
                                  const cell_range &target);

// Sort the rows or the columns of a rectangle.
std::unique_ptr<command> sort(sheet &where, const cell_range &range,
                              std::vector<book::sort_key> keys, bool by_rows);

// Insert or delete rows.
std::unique_ptr<command> change_rows(sheet &where, int at, int count,
                                     bool insert);

// Insert or delete columns.
std::unique_ptr<command> change_cols(sheet &where, int at, int count,
                                     bool insert);

// Resize columns or rows.
std::unique_ptr<command> resize(sheet &where, bool cols, int first, int last,
                                double points);

// Hide or show columns or rows.
std::unique_ptr<command> set_hidden(sheet &where, bool cols, int first,
                                    int last, bool hidden);

// Merge or unmerge a rectangle.
std::unique_ptr<command> merge_cells(sheet &where, const cell_range &range,
                                     bool merge);

// Add a sheet at an index.
std::unique_ptr<command> add_sheet(int index, std::string name);

// Delete a sheet by index.
std::unique_ptr<command> remove_sheet(int index);

// Rename a sheet by index.
std::unique_ptr<command> rename_sheet(int index, std::string name);

// Move a sheet to another index.
std::unique_ptr<command> move_sheet(int from, int to);

// Define a name, or remove it with a null formula.
std::unique_ptr<command> define_name(std::string name, expr::node_ptr formula,
                                     sheet *scope, cell_pos origin);

// Replace text across cells.
std::unique_ptr<command> replace_all(sheet &where, std::string text,
                                     std::string replacement,
                                     book::search_options options);

} // namespace cmd

#endif
