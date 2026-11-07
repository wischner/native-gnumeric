//
// The grid surface: a Native canvas showing a view::grid_view. It turns
// pointer and keyboard input into view operations and paints through a
// back buffer, so scrolling over unchanged cells stays cheap. It owns the
// workbook, its history and the view, so opening a file replaces them
// together.
//
// Derived from Gnumeric src/gnm-pane.c and src/item-grid.c,
// Copyright (C) 1998-2008 Miguel de Icaza, Jody Goldberg, Morten Welinder.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#ifndef UI_GRID_CANVAS_H
#define UI_GRID_CANVAS_H

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string_view>

#include <native.h>

#include "book/workbook.h"
#include "cmd/command.h"
#include "native_graphics.h"
#include "view/grid_view.h"

class desktop_test;

namespace ui {

// Shows and edits one workbook.
class grid_canvas final : public native::canvas {
public:
    // Create a canvas drawing with a catalog's fonts, showing a new book.
    explicit grid_canvas(font_catalog &fonts);

    // Show a workbook, taking ownership.
    void open(std::unique_ptr<book::workbook> source);

    // Return the view being edited.
    view::grid_view &grid() { return *view_; }

    // Return the workbook.
    book::workbook &workbook() { return *book_; }

    // Return where the workbook is saved; empty before the first save.
    const std::filesystem::path &path() const { return path_; }

    // Set where the workbook is saved.
    void set_path(std::filesystem::path path) { path_ = std::move(path); }

    // Bring scrolling and painting up to date, and tell the window.
    void refresh();

    // Set what runs after anything the user changes.
    void set_change_handler(std::function<void()> handler);

    // Set what runs when a key combination names a command.
    void set_command_handler(std::function<void(std::string_view)> handler);

private:
    // Drives the window through Native's public input entry points.
    friend class ::desktop_test;

    void paint(native::wnd_paint_event event);
    void click(native::mouse_event event);
    void drag(native::point position);
    bool key(native::key_event event);
    bool editing_key(native::key_event event, bool ctrl, bool shift);
    bool grid_key(native::key_event event, bool ctrl, bool shift);
    void point_key(int cols, int rows, bool extend);
    bool text(const std::string &utf8);
    void scrolled();
    void sync_viewport();

    // Return the content position of a host-relative point.
    native::point to_content(native::point position) const;

    font_catalog &fonts_;
    std::unique_ptr<book::workbook> book_;
    std::unique_ptr<cmd::command_stack> stack_;
    std::unique_ptr<view::grid_view> view_;
    std::filesystem::path path_;
    std::unique_ptr<native::img> buffer_;
    bool buffer_dirty_ = true;

    // An active pointer gesture.
    enum class gesture { none, select, fill, point };
    gesture gesture_ = gesture::none;
    val::cell_pos gesture_anchor_;
    std::optional<val::cell_pos> key_point_;

    std::function<void()> on_change_;
    std::function<void(std::string_view)> on_command_;
};

} // namespace ui

#endif
