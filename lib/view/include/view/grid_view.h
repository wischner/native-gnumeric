//
// The interactive state of a workbook window: which sheet shows, where
// the cursor and selection are, what is scrolled into view, the cell
// being edited, and what is on the clipboard. It translates gestures
// into commands and paints the grid. Everything here is testable with no
// display; src/ui only forwards input and presents the result, so a menu
// item, a toolbar button and a key binding all run this code.
//
// Derived from Gnumeric src/sheet-view.c, src/selection.c,
// src/wbc-gtk-edit.c and src/sheet-control-gui.c,
// Copyright (C) 1998-2003 Miguel de Icaza, Jody Goldberg, Morten Welinder.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#ifndef VIEW_GRID_VIEW_H
#define VIEW_GRID_VIEW_H

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "book/region.h"
#include "book/workbook.h"
#include "cmd/command.h"
#include "cmd/edits.h"
#include "gfx/graphics.h"
#include "view/editor.h"

namespace view {

using book::sheet;
using val::cell_pos;
using val::cell_range;

// Device pixels at 100% zoom: 96 per inch.
inline constexpr double pixels_per_point = 96.0 / 72.0;

// Where a pending paste comes from.
struct clip_source {
    book::region cells;
    std::string sheet_name;
    cell_range range;
    bool cut = false;
    std::string text;
};

// The part of the grid under a point.
enum class hit_kind { cell, fill_handle };

// What a point hits.
struct grid_hit {
    cell_pos pos;
    hit_kind kind = hit_kind::cell;
};

// How a key leaves the editor.
enum class commit_move { stay, down, up, right, left };

// One window's view of a workbook.
class grid_view {
public:
    // Create a view of a workbook's first sheet.
    grid_view(book::workbook &source, cmd::command_stack &stack,
              gfx::font_provider &fonts);

    book::workbook &workbook() { return *book_; }
    cmd::command_stack &commands() { return *stack_; }

    // Return the sheet showing.
    sheet &active() { return book_->sheet_at(active_); }

    // Return the index of the sheet showing.
    int active_index() const { return active_; }

    // Show another sheet, keeping each sheet's own cursor.
    void set_active(int index);

    // Make the view fit a changed sheet list, after undo for instance.
    void sync_sheets();

    double zoom() const { return zoom_; }

    // Set the zoom, clamped to 10% to 500%.
    void set_zoom(double zoom);

    // Tell the view its visible size.
    void set_viewport(int width, int height);

    int viewport_width() const { return viewport_width_; }
    int viewport_height() const { return viewport_height_; }

    // Scroll to a content position, clamped.
    void set_scroll(int x, int y);

    int scroll_x() const { return scroll_x_; }
    int scroll_y() const { return scroll_y_; }

    // Return the content size in device pixels.
    long long content_width() const;
    long long content_height() const;

    // Return a cell's rectangle in content pixels, merges included.
    gfx::device_rect cell_rect(cell_pos pos);

    // Return the rectangle of a range in content pixels.
    gfx::device_rect range_rect(const cell_range &range);

    // Return the cell at a content position.
    cell_pos cell_at(int x, int y);

    // Return what a content position hits.
    grid_hit hit_at(int x, int y);

    // Return the cursor.
    cell_pos cursor() const { return cursor_; }

    // Return the selected ranges; the last is the one being extended.
    const std::vector<cell_range> &selection() const { return ranges_; }

    // Return the range being extended.
    cell_range current_range() const { return ranges_.back(); }

    // Report whether more than one cell is selected.
    bool has_range() const;

    // Put the cursor on a cell, selecting it or extending the selection.
    void move_to(cell_pos pos, bool extend);

    // Start another selected range at a cell, the Ctrl+click gesture.
    void add_selection(cell_pos pos);

    // Move the cursor by cells.
    void move_by(int cols, int rows, bool extend);

    // Move the cursor to the edge of the data, Ctrl+arrow.
    void jump(int cols, int rows, bool extend);

    // Move the cursor by a page of rows.
    void move_page(int direction, bool extend);

    // Move the cursor to the start of the row, Home.
    void move_home(bool extend);

    // Select one column, a range of them, or add to the selection.
    void select_cols(int first, int last, bool add);

    // Select one row, a range of them, or add to the selection.
    void select_rows(int first, int last, bool add);

    // Select the sheet.
    void select_all();

    // Scroll so the cursor shows.
    void reveal_cursor();

    // Report whether a cell is being edited.
    bool editing() const { return editor_.active(); }

    cell_editor &editor() { return editor_; }

    // Open the editor on the cursor's content, F2.
    void begin_edit();

    // Open the editor replacing the content, by typing.
    void begin_typing(std::string_view text);

    // Insert text at the editor's caret, opening it if needed.
    void insert_text(std::string_view text);

    // Commit the edit and move the cursor; a bad formula stays open.
    void commit(commit_move move = commit_move::stay);

    // Commit the edit as an array formula over the selection.
    void commit_array();

    // Commit the edit into every selected cell, Ctrl+Enter.
    void commit_fill();

    // Abandon the edit.
    void cancel_edit();

    // Report whether a key or click would write a reference.
    bool can_point() const { return editor_.can_point(); }

    // Write a reference to a cell or range into the formula being typed.
    void point_at(const cell_range &range);

    // Return the range being pointed at.
    const std::optional<cell_range> &pointed() const { return pointed_; }

    // Return why the last commit failed; empty when it did not.
    const std::string &edit_error() const { return edit_error_; }

    // Return the text a cell shows.
    std::string display(cell_pos pos);

    // Return the entry the cursor's cell holds.
    std::string cursor_entry();

    // Return the cursor's name, or the range's while dragging.
    std::string position_name() const;

    // Return the text for the status bar: the sum of the selection.
    std::string selection_summary();

    //
    // Remember the selection for pasting.
    //
    // Returns:
    //      The selection as text, for the system clipboard.
    //
    std::string copy();

    // Remember the selection to be moved by the next paste.
    std::string cut();

    //
    // Paste at the selection.
    //
    // Parameters:
    //      system_text - What the system clipboard holds. When it is what
    //                    this view put there, the cells come back with
    //                    formulas and formats; otherwise it is plain text.
    //
    void paste(const std::string &system_text);

    // Paste with options from the view's own clipboard.
    void paste_special(const book::paste_options &options);

    // Return the pending clipboard, for drawing its frame.
    const std::optional<clip_source> &clipboard() const { return clip_; }

    // Forget the pending clipboard, Escape.
    void clear_clipboard();

    // Fill the selection from a dragged handle.
    void fill_to(const cell_range &target);

    // Remember a fill-handle drag's target for drawing.
    void set_fill_preview(std::optional<cell_range> target);

    // Open the editor on a SUM of the numbers above or left of the cursor.
    void auto_sum();

    // Clear contents, formats or both from the selection.
    void clear_selection(cmd::clear_kind kind);

    // Change the style of the selection.
    void format(const book::style_patch &patch, std::string name);

    // Return the cursor cell's style.
    const book::style &cursor_style() const;

    // Insert rows above the selection.
    void insert_rows();

    // Insert columns left of the selection.
    void insert_cols();

    // Delete the selected rows.
    void delete_rows();

    // Delete the selected columns.
    void delete_cols();

    // Set the width of the selected columns, or the given one.
    void set_col_width(int first, int last, double points);

    // Set the height of the selected rows, or the given one.
    void set_row_height(int first, int last, double points);

    // Size columns to their content.
    void autofit_cols(int first, int last);

    // Size rows to their content.
    void autofit_rows(int first, int last);

    // Hide or show the selected columns or rows.
    void set_hidden(bool cols, bool hidden);

    // Merge the selection, or unmerge it when already merged.
    void toggle_merge();

    // Sort the selection's rows by the cursor's column.
    void sort_selection(bool ascending);

    // Add a sheet after the active one and show it.
    void add_sheet();

    // Delete the active sheet.
    void remove_sheet();

    // Rename the active sheet; false when the name is taken or empty.
    bool rename_sheet(const std::string &name);

    // Move the active sheet by a number of places.
    void move_sheet(int places);

    // Save the cursor and selection into the sheet for saving the file.
    void store_selection();

    // Undo or redo, then bring the view up to date.
    void undo();
    void redo();

    // Paint the visible part of the grid into a viewport-sized target.
    void draw(gfx::painter &target);

    // Set what runs after the view changes without a user gesture.
    void set_change_handler(std::function<void()> handler) {
        on_change_ = std::move(handler);
    }

private:
    friend class grid_renderer;

    // Bring the sheet's axes to the zoom.
    void apply_scale();

    // Restore a sheet's cursor when it shows.
    void load_selection();

    // Keep the scroll inside the content.
    void clamp_scroll();

    // Return a cell clamped to the sheet.
    cell_pos clamp(cell_pos pos) const;

    // Return the range between the anchor and a cell, merges grown.
    cell_range extended_to(cell_pos pos);

    // Grow a range over the merges it touches.
    cell_range grown(cell_range range) const;

    // Run a command and keep the view in step with it.
    void run(std::unique_ptr<cmd::command> action);

    book::workbook *book_;
    cmd::command_stack *stack_;
    gfx::font_provider *fonts_;
    int active_ = 0;
    double zoom_ = 1.0;
    int viewport_width_ = 640;
    int viewport_height_ = 480;
    int scroll_x_ = 0;
    int scroll_y_ = 0;
    cell_pos cursor_;
    cell_pos anchor_;
    std::vector<cell_range> ranges_;
    cell_editor editor_;
    std::optional<cell_range> pointed_;
    std::optional<cell_pos> point_anchor_;
    std::optional<cell_range> fill_preview_;
    std::optional<clip_source> clip_;
    std::string edit_error_;
    std::function<void()> on_change_;
};

// Run a named command on a view; false when the name is not a command.
bool run_command(grid_view &target, std::string_view command);

} // namespace view

#endif
