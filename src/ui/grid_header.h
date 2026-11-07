//
// The column and row headers. Each is a Native ruler strip whose
// painting and pointer handling are the grid's: it keeps the ruler's
// docking and the shared corner, so the grid sits inside its headers as
// Writer's page sits inside its rulers. Selecting and resizing lines
// happen here.
//
// Derived from Gnumeric src/item-bar.c,
// Copyright (C) 1998-2003 Miguel de Icaza, Jody Goldberg, Morten Welinder.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#ifndef UI_GRID_HEADER_H
#define UI_GRID_HEADER_H

#include <optional>

#include <native.h>

#include "grid_canvas.h"

class desktop_test;

namespace ui {

// One edge's labels for columns or rows.
class grid_header final : public native::ruler {
public:
    // Attach a header strip; top shows columns, left shows rows.
    grid_header(grid_canvas &grid, native::window_edge edge, int extent);

    // Repaint after the view changes.
    void refresh();

protected:
    void draw(native::gpx &graphics, const native::rect &bounds) override;
    bool pointer_click(const native::mouse_event &event) override;
    void track_pointer(const native::point &position) override;
    void pointer_cancel() override;
    bool get_pointer_held() const override;

private:
    friend class ::desktop_test;

    // Return the viewport coordinate of a host-relative point.
    int local(const native::point &position) const;

    // Return the line under a viewport coordinate.
    int line_at(int position) const;

    // Return the line whose trailing edge is under a position, or -1.
    int edge_at(int position) const;

    // Return an axis of the active sheet.
    book::axis &axis_of() const;

    // Select lines from the anchor to one more.
    void select_to(int line, bool add);

    grid_canvas &grid_;
    bool columns_;

    // An active gesture: a resize of one line, or selecting lines.
    std::optional<int> resizing_;
    std::optional<int> selecting_;
    int resize_start_ = 0;
    int resize_pixels_ = 0;
};

} // namespace ui

#endif
