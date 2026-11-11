//
// Header painting and line gestures.
//
// Derived from Gnumeric src/item-bar.c,
// Copyright (C) 1998-2003 Miguel de Icaza, Jody Goldberg, Morten Welinder.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include "grid_header.h"

#include <algorithm>

namespace ui {
namespace {

using native::coord;
using native::dim;

constexpr int resize_reach = 3;
constexpr int smallest_line = 4;

// Report whether a line is covered by a selected range.
bool line_selected(view::grid_view &grid, bool columns, int line) {
    for (const val::cell_range &range : grid.selection())
        if (columns ? line >= range.start.col && line <= range.end.col
                    : line >= range.start.row && line <= range.end.row)
            return true;
    return false;
}

} // namespace

grid_header::grid_header(grid_canvas &grid, native::window_edge edge,
                         int extent)
    : native::ruler(grid, edge, extent), grid_(grid),
      columns_(edge == native::window_edge::top) {}

void grid_header::refresh() {
    invalidate();
}

book::axis &grid_header::axis_of() const {
    book::sheet &sheet = grid_.grid().active();
    return columns_ ? sheet.cols() : sheet.rows();
}

int grid_header::local(const native::point &position) const {
    const native::rect client = grid_.get_client_bounds();
    return columns_ ? position.x - client.p.x : position.y - client.p.y;
}

int grid_header::line_at(int position) const {
    view::grid_view &grid = grid_.grid();
    const int scroll = columns_ ? grid.scroll_x() : grid.scroll_y();
    return axis_of().index_at(position + scroll);
}

int grid_header::edge_at(int position) const {
    view::grid_view &grid = grid_.grid();
    const book::axis &axis = axis_of();
    const int scroll = columns_ ? grid.scroll_x() : grid.scroll_y();
    const int content = position + scroll;
    const int line = axis.index_at(content);
    for (const int candidate : {line, line - 1}) {
        if (candidate < 0)
            continue;
        const long long edge = axis.offset(candidate + 1);
        if (std::llabs(edge - content) <= resize_reach)
            return candidate;
    }
    return -1;
}

void grid_header::draw(native::gpx &graphics, const native::rect &bounds) {
    view::grid_view &grid = grid_.grid();
    book::axis &axis = axis_of();
    auto appearance = native::theme::create(graphics);
    appearance->draw_surface(bounds, native::surface_kind::table_header, {});
    const native::rect client = grid_.get_client_bounds();
    const int scroll = columns_ ? grid.scroll_x() : grid.scroll_y();
    const int length = columns_ ? client.d.w : client.d.h;
    const auto saved = graphics.save_state();
    graphics.set_clip(graphics.get_clip().intersect(bounds));
    graphics.set_font(native::font_t::stock(native::font_role::control));
    native::text_layout layout;
    layout.horizontal = native::text_align::center;
    layout.vertical = native::text_valign::center;
    const native::rgba rule = appearance->get_button_shadow_color();
    for (int line = axis.index_at(scroll);
         line < axis.count() && axis.offset(line) < scroll + length; ++line) {
        const int size = axis.pixels(line);
        if (size == 0)
            continue;
        const int start = static_cast<int>(axis.offset(line)) - scroll;
        const native::rect cell =
            columns_ ? native::rect(static_cast<coord>(client.p.x + start),
                                    bounds.p.y, static_cast<dim>(size),
                                    bounds.d.h)
                     : native::rect(bounds.p.x,
                                    static_cast<coord>(client.p.y + start),
                                    bounds.d.w, static_cast<dim>(size));
        const bool selected = line_selected(grid, columns_, line);
        native::theme::state state;
        state.selected = selected;
        if (selected)
            appearance->draw_selection(cell, native::selection_shape::row,
                                       state);
        graphics.set_ink(selected ? appearance->get_selection_foreground_color()
                                  : appearance->get_button_foreground_color());
        graphics.draw_text(columns_ ? val::col_name(line)
                                    : val::row_name(line),
                           cell, layout);
        graphics.set_ink(rule).set_pen(1);
        graphics.draw_border(cell, columns_ ? native::border_sides::right
                                            : native::border_sides::bottom);
    }
    // Keep the header-to-grid divider visible after selection fills.
    graphics.set_ink(rule).set_pen(1);
    graphics.draw_border(bounds, columns_ ? native::border_sides::bottom
                                          : native::border_sides::right);
}

void grid_header::select_to(int line, bool add) {
    view::grid_view &grid = grid_.grid();
    if (columns_)
        grid.select_cols(*selecting_, line, add);
    else
        grid.select_rows(*selecting_, line, add);
    grid_.refresh();
    refresh();
}

bool grid_header::pointer_click(const native::mouse_event &event) {
    if (event.button != native::mouse_button::left)
        return true;
    view::grid_view &grid = grid_.grid();
    const int position = local(event.position);
    if (event.action == native::mouse_action::release) {
        if (resizing_) {
            const double scale = grid.zoom() * view::pixels_per_point;
            const int pixels = std::max(
                smallest_line, resize_pixels_ + position - resize_start_);
            if (columns_)
                grid.set_col_width(*resizing_, *resizing_, pixels / scale);
            else
                grid.set_row_height(*resizing_, *resizing_, pixels / scale);
            grid_.refresh();
        }
        resizing_.reset();
        selecting_.reset();
        return true;
    }
    if (grid.editing()) {
        grid.commit();
        if (grid.editing())
            return true;
    }
    const int edge = edge_at(position);
    if (edge >= 0) {
        if (event.clicks == 2) {
            if (columns_)
                grid.autofit_cols(edge, edge);
            else
                grid.autofit_rows(edge, edge);
            grid_.refresh();
            return true;
        }
        resizing_ = edge;
        resize_start_ = position;
        resize_pixels_ = axis_of().pixels(edge);
        return true;
    }
    const int line = line_at(position);
    selecting_ = line;
    select_to(line,
              (event.modifiers & native::key_modifiers::ctrl) ==
                  native::key_modifiers::ctrl);
    grid_.focus();
    return true;
}

void grid_header::track_pointer(const native::point &position) {
    if (selecting_)
        select_to(line_at(local(position)), false);
}

void grid_header::pointer_cancel() {
    resizing_.reset();
    selecting_.reset();
}

bool grid_header::get_pointer_held() const {
    return resizing_.has_value() || selecting_.has_value();
}

} // namespace ui
