//
// Shared theme metrics and geometry for application forms.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//
#ifndef UI_FORM_LAYOUT_H
#define UI_FORM_LAYOUT_H

#include <native.h>

namespace ui {

struct form_metrics {
    int line;
    int field;
    int button;
    int check;
    int gap;
    int margin;
    int button_width;
};

// Measure controls with the active theme and control font, as Vision Edit Form.
form_metrics measure_form(native::wnd &window);

// Keep the requested client area available, accounting for native window chrome.
void fit_form(native::wnd &window, int width, int height);

// Center a form on its owner while keeping it inside the display work area.
void center_form(native::app_wnd &window);

// Return the client area inside the form's outer margin.
native::rect form_area(native::wnd &window, const form_metrics &metrics);

// Paint a non-interactive label in the control font.
void paint_form_label(native::wnd_paint_event event, const std::string &text,
                      const native::rect &area);

// Attach, create and show a control with an explicit traversal order.
void attach_form_control(native::wnd &parent, native::wnd &child, unsigned order);

} // namespace ui
#endif
