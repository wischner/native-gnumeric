//
// Theme-sized forms shared by the input, function and search windows.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//
#include "form_layout.h"

#include <algorithm>

namespace ui {

form_metrics measure_form(native::wnd &window) {
    auto &graphics = window.get_gpx();
    auto saved = graphics.save_state();
    graphics.set_font(native::font_t::stock(native::font_role::control));
    const auto theme = native::theme::create(graphics);
    const int line = graphics.measure_text("Ag").height;
    const int gap = std::max(6, line / 2);
    const int form = theme->get_form_control_height();
    return {line, form > 0 ? form : theme->get_text_edit_height(),
            form > 0 ? form : theme->get_button_height(),
            std::max(line + gap, theme->get_check_height()), gap,
            gap * 2, std::max(88, graphics.measure_text("Replace All").width + 4 * gap)};
}

void fit_form(native::wnd &window, int width, int height) {
    const auto client = window.get_client_bounds();
    const auto size = window.get_dimensions();
    if (client.w() < width || client.h() < height)
        window.set_dimensions(native::size(
            size.w + std::max(0, width - int(client.w())),
            size.h + std::max(0, height - int(client.h()))));
}

void center_form(native::app_wnd &window) {
    window.center_to_parent();
    const auto &screens = native::screen::detect();
    if (screens.empty()) return;
    const auto bounds = window.get_bounds();
    const native::point center(bounds.x1() + bounds.w() / 2,
                               bounds.y1() + bounds.h() / 2);
    const native::screen *screen = &screens.front();
    for (const auto &candidate : screens)
        if (candidate.bounds().contains(center)) screen = &candidate;
    const auto area = screen->work_area();
    window.set_position(native::point(
        std::clamp(int(bounds.x1()), int(area.x1()),
                   std::max(int(area.x1()), int(area.x2()) - int(bounds.w()))),
        std::clamp(int(bounds.y1()), int(area.y1()),
                   std::max(int(area.y1()), int(area.y2()) - int(bounds.h())))));
}

native::rect form_area(native::wnd &window, const form_metrics &metrics) {
    const auto client = window.get_client_bounds();
    return native::rect(client.x1() + metrics.margin,
                        client.y1() + metrics.margin,
                        std::max(1, int(client.w()) - 2 * metrics.margin),
                        std::max(1, int(client.h()) - 2 * metrics.margin));
}

void paint_form_label(native::wnd_paint_event event, const std::string &text,
                      const native::rect &area) {
    auto saved = event.g.save_state();
    event.g.set_ink(native::theme::create(event.g)->get_button_foreground_color());
    event.g.set_font(native::font_t::stock(native::font_role::control));
    native::text_layout layout;
    layout.vertical = native::text_valign::center;
    event.g.draw_text(text, area, layout);
}

void attach_form_control(native::wnd &parent, native::wnd &child, unsigned order) {
    child.set_tab_order(order);
    child.set_parent(&parent);
    child.create();
    child.show();
}

} // namespace ui
