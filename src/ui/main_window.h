//
// The application window: menus, toolbar, format bar, formula bar,
// headers, sheet tabs, status bar and the grid, all Native controls. It
// is laid out as Writer's window is, so the two read as one suite.
//
// Derived from Gnumeric src/wbc-gtk.c and src/wbc-gtk-actions.c,
// Copyright (C) 2000-2006 Jody Goldberg, Morten Welinder and others.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#ifndef UI_MAIN_WINDOW_H
#define UI_MAIN_WINDOW_H

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <native.h>

#include "app/options.h"
#include "dialogs.h"
#include "find_window.h"
#include "formula_bar.h"
#include "grid_canvas.h"
#include "grid_header.h"
#include "impexp/file_format.h"
#include "native_graphics.h"

class desktop_test;

namespace ui {

// One workbook window.
class main_window final : public native::app_wnd {
public:
    // Build the window for the given command line.
    explicit main_window(const app::options &settings);

    void request_close() override;

    // Return the grid.
    native::canvas &canvas_probe() { return canvas_; }

private:
    // Drives the window through Native's public input entry points.
    friend class ::desktop_test;

    void build_menu();
    void build_toolbar();
    void build_format_bar();
    void build_formula_strip();
    void place_children();
    void run(std::string_view command);
    bool run_window_command(std::string_view command);
    void update_state();
    void update_format_controls();
    void update_tabs();
    void update_title();
    void set_message(const std::string &text);

    void new_document();
    void load(const std::filesystem::path &path);
    bool save();
    bool save_to(const std::filesystem::path &path);
    void open_requested();
    void save_as_requested();
    bool confirm_discard();
    void write_clipboard(const std::string &text);
    std::string read_clipboard();
    void paste();
    void set_zoom(double zoom);
    void show_find();
    void show_about();
    void show_goto();
    void go_to(const std::string &target);
    void show_rename();
    void show_size(bool columns);
    void show_custom_format();
    void show_define_name();
    void show_function();

    app::options settings_;
    font_catalog fonts_;

    native::combo_box number_box_;
    native::combo_box font_box_;
    native::combo_box size_box_;
    native::combo_box color_box_;
    native::combo_box fill_box_;
    native::combo_box zoom_box_;
    grid_canvas canvas_;
    formula_bar formula_;
    native::toolbar toolbar_;
    native::toolbar format_bar_;
    native::toolbar formula_strip_;
    native::tab_view tabs_;
    native::toolbar sheet_bar_;
    grid_header col_header_;
    grid_header row_header_;
    native::status_bar status_;
    native::open_file_dialog open_dialog_;
    native::save_file_dialog save_dialog_;
    find_window find_;
    input_window input_;
    function_window function_;

    // Command name of each menu id; ids are indices plus one.
    std::vector<std::string> menu_commands_;

    std::string message_;
    std::vector<std::string> shown_tabs_;
    bool updating_ = false;
};

} // namespace ui

#endif
