//
// Paints the formula bar with the theme's field frames, and starts an
// edit where it is clicked.
//
// Derived from Gnumeric src/wbc-gtk-edit.c,
// Copyright (C) 2000-2002 Jon K Hellan, Jody Goldberg, Morten Welinder.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include "formula_bar.h"

#include <algorithm>

#include "util/utf8.h"

namespace ui {
namespace {

using native::coord;
using native::dim;

constexpr int name_width = 96;
constexpr int gap = 4;
constexpr int text_inset = 4;

// Return the bar's text font.
const native::font_t &bar_font() {
    return native::font_t::stock(native::font_role::control);
}

} // namespace

formula_bar::formula_bar(grid_canvas &grid)
    : native::canvas(0, 0, 400, 24), grid_(grid) {
    set_cursor(native::mouse_cursor::ibeam);
    set_horizontal_scrollbar_policy(native::scrollbar_policy::never);
    set_vertical_scrollbar_policy(native::scrollbar_policy::never);
    on_wnd_paint.connect([this](native::wnd_paint_event event) {
        paint(event);
        return true;
    });
    on_mouse_click.connect([this](native::mouse_event event) {
        click(event);
        return true;
    });
}

void formula_bar::set_name_handler(std::function<void()> handler) {
    on_name_ = std::move(handler);
}

void formula_bar::refresh() {
    invalidate();
}

native::rect formula_bar::text_area() const {
    const native::size size = get_dimensions();
    return native::rect(
        static_cast<coord>(name_width + gap), 0,
        static_cast<dim>(std::max(0, size.w - name_width - gap)), size.h);
}

void formula_bar::paint(native::wnd_paint_event event) {
    native::gpx &g = event.g;
    auto appearance = native::theme::create(g);
    const native::size size = get_dimensions();
    appearance->draw_surface(native::rect(0, 0, size.w, size.h),
                             native::surface_kind::panel, {});
    view::grid_view &grid = grid_.grid();
    const native::rect name(0, 0, name_width, size.h);
    const native::rect body = text_area();
    native::theme::state field;
    appearance->draw_text_edit_frame(name, field);
    field.selected = grid.editing();
    appearance->draw_text_edit_frame(body, field);
    const auto saved = g.save_state();
    g.set_font(bar_font());
    g.set_ink(appearance->get_content_foreground_color());
    native::text_layout layout;
    layout.vertical = native::text_valign::center;
    layout.horizontal = native::text_align::center;
    g.draw_text(grid.position_name(), name, layout);
    const std::string entry =
        grid.editing() ? grid.editor().text() : grid.cursor_entry();
    const native::rect inner(static_cast<coord>(body.p.x + text_inset),
                             body.p.y,
                             static_cast<dim>(
                                 std::max(0, body.d.w - 2 * text_inset)),
                             body.d.h);
    layout.horizontal = native::text_align::start;
    g.set_clip(g.get_clip().intersect(inner));
    g.draw_text(entry, inner, layout);
    if (grid.editing()) {
        const std::string head = util::to_utf8(
            grid.editor().characters().substr(0, grid.editor().caret()));
        const int x = inner.p.x + bar_font().measure_text(head).advance;
        g.set_pen(1);
        g.draw_line(native::point(static_cast<coord>(x), 3),
                    native::point(static_cast<coord>(x),
                                  static_cast<coord>(size.h - 4)));
    }
}

std::size_t formula_bar::caret_at(int x) const {
    const std::u32string &text = grid_.grid().editor().characters();
    const native::rect body = text_area();
    const int wanted = x - body.p.x - text_inset;
    for (std::size_t i = 1; i <= text.size(); ++i) {
        const int width =
            bar_font().measure_text(util::to_utf8(text.substr(0, i))).advance;
        if (width > wanted)
            return i - 1;
    }
    return text.size();
}

void formula_bar::click(native::mouse_event event) {
    if (event.button != native::mouse_button::left ||
        event.action != native::mouse_action::press)
        return;
    if (event.position.x < name_width) {
        if (on_name_)
            on_name_();
        return;
    }
    view::grid_view &grid = grid_.grid();
    grid.begin_edit();
    grid.editor().set_caret(caret_at(event.position.x), false);
    grid_.focus();
    grid_.refresh();
}

} // namespace ui
