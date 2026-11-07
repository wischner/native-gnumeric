//
// Cursor, selection, scrolling and sheet switching.
//
// Derived from Gnumeric src/sheet-view.c and src/selection.c,
// Copyright (C) 1998-2003 Miguel de Icaza, Jody Goldberg, Morten Welinder.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include "view/grid_view.h"

#include <algorithm>
#include <cmath>

namespace view {
namespace {

// The handle at a selection's corner is this far around the corner.
constexpr int handle_reach = 4;

} // namespace

grid_view::grid_view(book::workbook &source, cmd::command_stack &stack,
                     gfx::font_provider &fonts)
    : book_(&source), stack_(&stack), fonts_(&fonts) {
    ranges_.push_back(cell_range::single({0, 0}));
    apply_scale();
    load_selection();
}

void grid_view::apply_scale() {
    const double scale = zoom_ * pixels_per_point;
    active().cols().set_scale(scale);
    active().rows().set_scale(scale);
}

void grid_view::load_selection() {
    const book::saved_selection &saved = active().selection;
    cursor_ = clamp(saved.cursor);
    anchor_ = cursor_;
    ranges_ = saved.ranges;
    if (ranges_.empty())
        ranges_.push_back(cell_range::single(cursor_));
}

void grid_view::store_selection() {
    book::saved_selection &saved = active().selection;
    saved.cursor = cursor_;
    saved.ranges = ranges_;
}

void grid_view::set_active(int index) {
    if (index == active_ || index < 0 || index >= book_->sheet_count())
        return;
    if (editing())
        commit();
    store_selection();
    active_ = index;
    apply_scale();
    load_selection();
    scroll_x_ = scroll_y_ = 0;
    reveal_cursor();
}

void grid_view::sync_sheets() {
    active_ = std::min(active_, book_->sheet_count() - 1);
    apply_scale();
    cursor_ = clamp(cursor_);
    clamp_scroll();
}

void grid_view::set_zoom(double zoom) {
    zoom_ = std::clamp(zoom, 0.1, 5.0);
    apply_scale();
    clamp_scroll();
}

void grid_view::set_viewport(int width, int height) {
    viewport_width_ = width;
    viewport_height_ = height;
    clamp_scroll();
}

long long grid_view::content_width() const {
    return const_cast<grid_view *>(this)->active().cols().total();
}

long long grid_view::content_height() const {
    return const_cast<grid_view *>(this)->active().rows().total();
}

void grid_view::clamp_scroll() {
    const long long max_x =
        std::max<long long>(0, content_width() - viewport_width_);
    const long long max_y =
        std::max<long long>(0, content_height() - viewport_height_);
    scroll_x_ = static_cast<int>(
        std::clamp<long long>(scroll_x_, 0, max_x));
    scroll_y_ = static_cast<int>(
        std::clamp<long long>(scroll_y_, 0, max_y));
}

void grid_view::set_scroll(int x, int y) {
    scroll_x_ = x;
    scroll_y_ = y;
    clamp_scroll();
}

cell_pos grid_view::clamp(cell_pos pos) const {
    book::workbook &book = *book_;
    const sheet &item = book.sheet_at(active_);
    return {std::clamp(pos.col, 0, item.col_count() - 1),
            std::clamp(pos.row, 0, item.row_count() - 1)};
}

gfx::device_rect grid_view::range_rect(const cell_range &range) {
    sheet &item = active();
    const long long left = item.cols().offset(range.start.col);
    const long long top = item.rows().offset(range.start.row);
    const long long right = item.cols().offset(range.end.col + 1);
    const long long bottom = item.rows().offset(range.end.row + 1);
    return {static_cast<int>(left), static_cast<int>(top),
            static_cast<int>(right - left), static_cast<int>(bottom - top)};
}

gfx::device_rect grid_view::cell_rect(cell_pos pos) {
    if (const cell_range *merge = active().merged_at(pos))
        return range_rect(*merge);
    return range_rect(cell_range::single(pos));
}

cell_pos grid_view::cell_at(int x, int y) {
    sheet &item = active();
    return {item.cols().index_at(x), item.rows().index_at(y)};
}

grid_hit grid_view::hit_at(int x, int y) {
    grid_hit hit{cell_at(x, y), hit_kind::cell};
    const gfx::device_rect edges = range_rect(ranges_.back());
    const int corner_x = edges.x + edges.width;
    const int corner_y = edges.y + edges.height;
    if (std::abs(x - corner_x) <= handle_reach &&
        std::abs(y - corner_y) <= handle_reach)
        hit.kind = hit_kind::fill_handle;
    return hit;
}

bool grid_view::has_range() const {
    return ranges_.size() > 1 || !ranges_.back().is_single();
}

cell_range grid_view::grown(cell_range range) const {
    bool changed = true;
    const sheet &item = book_->sheet_at(active_);
    while (changed) {
        changed = false;
        for (const cell_range &merge : item.merged())
            if (merge.overlaps(range) && !range.contains(merge)) {
                range.start.col = std::min(range.start.col, merge.start.col);
                range.start.row = std::min(range.start.row, merge.start.row);
                range.end.col = std::max(range.end.col, merge.end.col);
                range.end.row = std::max(range.end.row, merge.end.row);
                changed = true;
            }
    }
    return range;
}

cell_range grid_view::extended_to(cell_pos pos) {
    return grown(cell_range::between(anchor_, pos));
}

void grid_view::move_to(cell_pos pos, bool extend) {
    pos = clamp(pos);
    cursor_ = pos;
    if (extend) {
        ranges_.back() = extended_to(pos);
    } else {
        anchor_ = pos;
        ranges_.assign(1, grown(cell_range::single(pos)));
    }
}

void grid_view::add_selection(cell_pos pos) {
    pos = clamp(pos);
    cursor_ = anchor_ = pos;
    ranges_.push_back(grown(cell_range::single(pos)));
}

void grid_view::move_by(int cols, int rows, bool extend) {
    sheet &item = active();
    cell_pos next = cursor_;
    // Step over hidden lines and off a merge, not into it.
    if (const cell_range *merge = item.merged_at(next); merge && !extend) {
        if (cols > 0)
            next.col = merge->end.col;
        if (rows > 0)
            next.row = merge->end.row;
        if (cols < 0)
            next.col = merge->start.col;
        if (rows < 0)
            next.row = merge->start.row;
    }
    next.col += cols;
    next.row += rows;
    next = clamp(next);
    while (cols != 0 && item.cols().is_hidden(next.col) &&
           next.col + cols >= 0 && next.col + cols < item.col_count())
        next.col += cols > 0 ? 1 : -1;
    while (rows != 0 && item.rows().is_hidden(next.row) &&
           next.row + rows >= 0 && next.row + rows < item.row_count())
        next.row += rows > 0 ? 1 : -1;
    move_to(next, extend);
    reveal_cursor();
}

void grid_view::jump(int cols, int rows, bool extend) {
    sheet &item = active();
    const auto has_content = [&](cell_pos pos) {
        const book::cell *found = item.find(pos);
        return found && !found->result.is_empty();
    };
    cell_pos next = cursor_;
    const auto step = [&](cell_pos pos) {
        return clamp({pos.col + cols, pos.row + rows});
    };
    if (step(next) == next)
        return;
    if (has_content(next) && has_content(step(next))) {
        // Along filled cells to the last one.
        while (step(next) != next && has_content(step(next)))
            next = step(next);
    } else {
        // Across empty cells to the next filled one, or the sheet's edge.
        next = step(next);
        while (step(next) != next && !has_content(next))
            next = step(next);
    }
    move_to(next, extend);
    reveal_cursor();
}

void grid_view::move_page(int direction, bool extend) {
    sheet &item = active();
    const int page = std::max(
        1, viewport_height_ /
               std::max(1, item.rows().pixels(cursor_.row)) - 1);
    scroll_y_ += direction * viewport_height_;
    clamp_scroll();
    move_to({cursor_.col, cursor_.row + direction * page}, extend);
    reveal_cursor();
}

void grid_view::move_home(bool extend) {
    move_to({0, cursor_.row}, extend);
    reveal_cursor();
}

void grid_view::select_cols(int first, int last, bool add) {
    const sheet &item = book_->sheet_at(active_);
    const cell_range range{{std::min(first, last), 0},
                           {std::max(first, last), item.row_count() - 1}};
    if (!add)
        ranges_.clear();
    ranges_.push_back(range);
    cursor_ = {first, 0};
    anchor_ = {first, 0};
}

void grid_view::select_rows(int first, int last, bool add) {
    const sheet &item = book_->sheet_at(active_);
    const cell_range range{{0, std::min(first, last)},
                           {item.col_count() - 1, std::max(first, last)}};
    if (!add)
        ranges_.clear();
    ranges_.push_back(range);
    cursor_ = {0, first};
    anchor_ = {0, first};
}

void grid_view::select_all() {
    const sheet &item = book_->sheet_at(active_);
    ranges_.assign(1, cell_range{{0, 0},
                                 {item.col_count() - 1, item.row_count() - 1}});
}

void grid_view::reveal_cursor() {
    const gfx::device_rect cell = cell_rect(cursor_);
    if (cell.x < scroll_x_)
        scroll_x_ = cell.x;
    else if (cell.x + cell.width > scroll_x_ + viewport_width_)
        scroll_x_ = std::min(cell.x, cell.x + cell.width - viewport_width_);
    if (cell.y < scroll_y_)
        scroll_y_ = cell.y;
    else if (cell.y + cell.height > scroll_y_ + viewport_height_)
        scroll_y_ = std::min(cell.y, cell.y + cell.height - viewport_height_);
    clamp_scroll();
}

} // namespace view
