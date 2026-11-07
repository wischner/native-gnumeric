//
// The formula bar: where the cursor is, and what its cell holds. It
// shows the view's one editor and leaves the keyboard on the grid, so
// typing here and typing in the cell are the same code. A plain canvas
// keeps that true on every backend: Native's text_edit has no caret
// access for writing a pointed-at reference at the insertion point.
//
// Derived from Gnumeric src/wbc-gtk-edit.c and
// src/widgets/gnm-expr-entry.c,
// Copyright (C) 2000-2002 Jon K Hellan, Jody Goldberg, Morten Welinder.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#ifndef UI_FORMULA_BAR_H
#define UI_FORMULA_BAR_H

#include <functional>
#include <string>

#include <native.h>

#include "grid_canvas.h"

class desktop_test;

namespace ui {

// Shows the cursor's name and entry.
class formula_bar final : public native::canvas {
public:
    // Create a bar over a grid.
    explicit formula_bar(grid_canvas &grid);

    // Set what runs when the name box is clicked.
    void set_name_handler(std::function<void()> handler);

    // Repaint after the view changes.
    void refresh();

private:
    friend class ::desktop_test;

    void paint(native::wnd_paint_event event);
    void click(native::mouse_event event);

    // Return the text area's bounds.
    native::rect text_area() const;

    // Return the character under an x position of the text.
    std::size_t caret_at(int x) const;

    grid_canvas &grid_;
    std::function<void()> on_name_;
};

} // namespace ui

#endif
