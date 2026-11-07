//
// Editing a cell, the clipboard and the gestures that become commands.
//
// Derived from Gnumeric src/wbc-gtk-edit.c, src/cmd-edit.c and
// src/clipboard.c,
// Copyright (C) 1998-2008 Miguel de Icaza, Jody Goldberg, Morten Welinder.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "book/fill.h"
#include "expr/parser.h"
#include "numfmt/format.h"
#include "util/text.h"
#include "view/grid_view.h"

namespace view {

void grid_view::run(std::unique_ptr<cmd::command> action) {
    stack_->run(std::move(action));
}

void grid_view::begin_edit() {
    if (editing())
        return;
    const book::cell *found = active().find(cursor_);
    if (found && active().style_at(cursor_)->locked && active().protect)
        return;
    edit_error_.clear();
    editor_.open(active().entry_text(cursor_));
    reveal_cursor();
}

void grid_view::begin_typing(std::string_view text) {
    edit_error_.clear();
    editor_.open("");
    editor_.insert(text);
    reveal_cursor();
}

void grid_view::insert_text(std::string_view text) {
    if (!editing())
        begin_typing(text);
    else
        editor_.insert(text);
    pointed_.reset();
    point_anchor_.reset();
}

void grid_view::commit(commit_move move) {
    if (!editing())
        return;
    std::string text = editor_.text();
    if (!text.empty()) {
        const book::parsed_entry entry = active().parse_entry(cursor_, text);
        if (!entry.error.empty()) {
            edit_error_ = entry.error;
            return;
        }
    }
    edit_error_.clear();
    editor_.close();
    pointed_.reset();
    point_anchor_.reset();
    run(cmd::set_entry(active(), cursor_, std::move(text)));
    switch (move) {
    case commit_move::down:
        move_by(0, 1, false);
        break;
    case commit_move::up:
        move_by(0, -1, false);
        break;
    case commit_move::right:
        move_by(1, 0, false);
        break;
    case commit_move::left:
        move_by(-1, 0, false);
        break;
    case commit_move::stay:
        break;
    }
}

void grid_view::commit_array() {
    if (!editing())
        return;
    const std::string text = editor_.text();
    const expr::parse_pos where{&active(), ranges_.back().start, book_};
    expr::parse_result parsed = expr::parse_formula(text, where);
    if (!parsed.tree) {
        edit_error_ = parsed.error;
        return;
    }
    edit_error_.clear();
    editor_.close();
    pointed_.reset();
    run(cmd::set_array(active(), ranges_.back(), std::move(parsed.tree)));
}

void grid_view::commit_fill() {
    if (!editing())
        return;
    const std::string text = editor_.text();
    editor_.close();
    pointed_.reset();
    run(cmd::fill_entry(active(), ranges_.back(), text));
}

void grid_view::cancel_edit() {
    editor_.close();
    pointed_.reset();
    point_anchor_.reset();
    edit_error_.clear();
}

void grid_view::point_at(const cell_range &range) {
    pointed_ = range;
    editor_.set_reference(val::range_name(range));
}

std::string grid_view::display(cell_pos pos) {
    return book::display_text(active(), pos);
}

std::string grid_view::cursor_entry() {
    return active().entry_text(cursor_);
}

std::string grid_view::position_name() const {
    return val::cell_name(cursor_);
}

std::string grid_view::selection_summary() {
    double total = 0;
    bool any = false;
    sheet &item = active();
    for (const cell_range &range : ranges_)
        item.for_each_cell(range, [&](cell_pos, const val::value &stored) {
            if (stored.is_number()) {
                total += stored.number();
                any = true;
            }
        });
    if (!any)
        return "Sum=0";
    return "Sum=" + numfmt::render_general(total, 14);
}

std::string grid_view::copy() {
    clip_source source;
    source.cells = book::copy_region(active(), ranges_.back());
    source.sheet_name = active().name();
    source.range = ranges_.back();
    source.text = book::region_as_text(active(), ranges_.back());
    clip_ = std::move(source);
    return clip_->text;
}

std::string grid_view::cut() {
    std::string text = copy();
    clip_->cut = true;
    return text;
}

void grid_view::paste(const std::string &system_text) {
    if (clip_ && (system_text.empty() || system_text == clip_->text)) {
        if (clip_->cut) {
            auto *from = static_cast<sheet *>(
                book_->find_sheet(clip_->sheet_name));
            if (from)
                run(cmd::move_range(*from, clip_->range, active(), cursor_));
            clip_.reset();
            return;
        }
        paste_special({});
        return;
    }
    if (!system_text.empty())
        run(cmd::paste_text(active(), cursor_, system_text));
}

void grid_view::paste_special(const book::paste_options &options) {
    if (!clip_)
        return;
    run(cmd::paste(active(), ranges_.back(), clip_->cells, options));
    const int cols = options.transpose ? clip_->cells.rows : clip_->cells.cols;
    const int rows = options.transpose ? clip_->cells.cols : clip_->cells.rows;
    if (ranges_.back().is_single()) {
        const cell_pos corner = ranges_.back().start;
        ranges_.back() = cell_range{corner,
                                    clamp({corner.col + cols - 1,
                                           corner.row + rows - 1})};
    }
}

void grid_view::clear_clipboard() {
    clip_.reset();
}

void grid_view::fill_to(const cell_range &target) {
    fill_preview_.reset();
    if (target == ranges_.back())
        return;
    run(cmd::autofill(active(), ranges_.back(), target));
    ranges_.back() = target;
}

void grid_view::set_fill_preview(std::optional<cell_range> target) {
    fill_preview_ = std::move(target);
}

void grid_view::auto_sum() {
    if (editing())
        return;
    sheet &item = active();
    const auto is_number = [&](cell_pos pos) {
        const book::cell *found = item.find(pos);
        return found && found->result.is_number();
    };
    cell_range range = cell_range::single(cursor_);
    if (cursor_.row > 0 && is_number({cursor_.col, cursor_.row - 1})) {
        int first = cursor_.row - 1;
        while (first > 0 && is_number({cursor_.col, first - 1}))
            --first;
        range = {{cursor_.col, first}, {cursor_.col, cursor_.row - 1}};
    } else if (cursor_.col > 0 && is_number({cursor_.col - 1, cursor_.row})) {
        int first = cursor_.col - 1;
        while (first > 0 && is_number({first - 1, cursor_.row}))
            --first;
        range = {{first, cursor_.row}, {cursor_.col - 1, cursor_.row}};
    }
    if (range.contains(cursor_)) {
        begin_typing("=SUM()");
        editor_.move(-1, false);
        return;
    }
    begin_typing("=SUM(" + val::range_name(range) + ")");
}

void grid_view::clear_selection(cmd::clear_kind kind) {
    run(cmd::clear_cells(active(), ranges_, kind));
}

void grid_view::format(const book::style_patch &patch, std::string name) {
    run(cmd::format_cells(active(), ranges_, patch, std::move(name)));
    if (!patch.font_size && !patch.wrap)
        return;
    const cell_range &range = ranges_.back();
    const int last = std::min(range.end.row, range.start.row + 200);
    autofit_rows(range.start.row, last);
    if (!patch.font_size)
        return;
    // An empty row grows too, ready for what is typed into it.
    const double scale = zoom_ * pixels_per_point;
    sheet &item = active();
    for (int row = range.start.row; row <= last; ++row) {
        const book::style &look = *item.style_at({range.start.col, row});
        const auto face = fonts_->find({look.font_name,
                                        look.font_size * scale, look.bold,
                                        look.italic});
        const double needed =
            (face->ascent() + face->descent() + 2) / scale;
        if (!item.rows().is_hard(row) && needed > item.rows().points(row))
            item.rows().set_points(row, needed, false);
    }
}

const book::style &grid_view::cursor_style() const {
    return *book_->sheet_at(active_).style_at(cursor_);
}

void grid_view::insert_rows() {
    const cell_range &range = ranges_.back();
    run(cmd::change_rows(active(), range.start.row, range.rows(), true));
}

void grid_view::insert_cols() {
    const cell_range &range = ranges_.back();
    run(cmd::change_cols(active(), range.start.col, range.cols(), true));
}

void grid_view::delete_rows() {
    const cell_range range = ranges_.back();
    run(cmd::change_rows(active(), range.start.row, range.rows(), false));
    move_to({cursor_.col, range.start.row}, false);
}

void grid_view::delete_cols() {
    const cell_range range = ranges_.back();
    run(cmd::change_cols(active(), range.start.col, range.cols(), false));
    move_to({range.start.col, cursor_.row}, false);
}

void grid_view::set_col_width(int first, int last, double points) {
    run(cmd::resize(active(), true, first, last, points));
}

void grid_view::set_row_height(int first, int last, double points) {
    run(cmd::resize(active(), false, first, last, points));
}

void grid_view::autofit_cols(int first, int last) {
    sheet &item = active();
    const double scale = zoom_ * pixels_per_point;
    for (int col = first; col <= last; ++col) {
        int widest = 0;
        const cell_range line{{col, 0}, {col, item.row_count() - 1}};
        std::vector<cell_pos> cells;
        item.for_each_cell(line, [&](cell_pos pos, const val::value &) {
            cells.push_back(pos);
        });
        for (const cell_pos pos : cells) {
            const book::style &look = *item.style_at(pos);
            const auto face = fonts_->find({look.font_name,
                                            look.font_size * scale, look.bold,
                                            look.italic});
            widest = std::max(widest, face->measure(display(pos)));
        }
        if (widest > 0)
            run(cmd::resize(item, true, col, col, (widest + 8) / scale));
    }
}

void grid_view::autofit_rows(int first, int last) {
    sheet &item = active();
    const double scale = zoom_ * pixels_per_point;
    const auto used = item.used_range();
    for (int row = first; row <= last; ++row) {
        if (item.rows().is_hard(row))
            continue;
        double tallest = item.rows().default_points();
        const int end_col = used ? used->end.col : -1;
        for (int col = 0; col <= end_col; ++col) {
            if (!item.find({col, row}))
                continue;
            const book::style &look = *item.style_at({col, row});
            const auto face = fonts_->find({look.font_name,
                                            look.font_size * scale, look.bold,
                                            look.italic});
            int lines = 1;
            if (look.wrap) {
                const int width = item.cols().pixels(col) - 6;
                if (width > 0)
                    lines += face->measure(display({col, row})) / width;
            }
            tallest = std::max(
                tallest,
                lines * (face->ascent() + face->descent() + 2) / scale);
        }
        if (tallest != item.rows().points(row))
            item.rows().set_points(row, tallest, false);
    }
}

void grid_view::set_hidden(bool cols, bool hidden) {
    for (const cell_range &range : ranges_) {
        if (cols)
            run(cmd::set_hidden(active(), true, range.start.col, range.end.col,
                                hidden));
        else
            run(cmd::set_hidden(active(), false, range.start.row,
                                range.end.row, hidden));
    }
}

void grid_view::toggle_merge() {
    const cell_range range = ranges_.back();
    bool merged = false;
    for (const cell_range &item : active().merged())
        merged = merged || item.overlaps(range);
    if (merged) {
        run(cmd::merge_cells(active(), range, false));
    } else if (!range.is_single()) {
        run(cmd::merge_cells(active(), range, true));
    }
}

void grid_view::sort_selection(bool ascending) {
    cell_range range = ranges_.back();
    if (range.is_single()) {
        const auto used = active().used_range();
        if (!used)
            return;
        range = *used;
    }
    const int key = std::clamp(cursor_.col, range.start.col, range.end.col) -
                    range.start.col;
    run(cmd::sort(active(), range, {{key, ascending, false}}, true));
}

void grid_view::add_sheet() {
    const int index = active_ + 1;
    run(cmd::add_sheet(index, book_->unique_sheet_name()));
    set_active(index);
}

void grid_view::remove_sheet() {
    if (book_->sheet_count() <= 1)
        return;
    store_selection();
    run(cmd::remove_sheet(active_));
    sync_sheets();
    load_selection();
}

bool grid_view::rename_sheet(const std::string &name) {
    if (util::trim(name).empty())
        return false;
    if (book_->find_sheet(name) && book_->find_sheet(name) != &active())
        return false;
    run(cmd::rename_sheet(active_, name));
    return true;
}

void grid_view::move_sheet(int places) {
    const int target = std::clamp(active_ + places, 0,
                                  book_->sheet_count() - 1);
    if (target == active_)
        return;
    run(cmd::move_sheet(active_, target));
    active_ = target;
}

void grid_view::undo() {
    if (editing())
        cancel_edit();
    stack_->undo();
    sync_sheets();
}

void grid_view::redo() {
    stack_->redo();
    sync_sheets();
}

} // namespace view
