//
// Commands that need the window: files, clipboard, dialogs and zoom.
// Everything else goes straight to view::run_command, so a menu item, a
// toolbar button and a key binding all run the same code. The menus
// follow Writer's, with Data added for what a spreadsheet does.
//
// Derived from Gnumeric src/wbc-gtk-actions.c and src/GNOME_Gnumeric-gtk.xml,
// Copyright (C) 2003-2006 Jody Goldberg, Morten Welinder and others.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include <algorithm>
#include <cstdlib>
#include <set>

#include "expr/parser.h"
#include "impexp/file_format.h"
#include "main_window.h"

namespace ui {
namespace {

// One menu row; an empty command is a separator.
struct menu_row {
    const char *label;
    const char *command;
};

// One top-level menu.
struct menu_group {
    const char *title;
    std::vector<menu_row> rows;
};

// The menu bar.
const std::vector<menu_group> &menu_groups() {
    static const std::vector<menu_group> groups = {
        {"&File",
         {{"&New\tCtrl+N", "fileNew"},
          {"&Open...\tCtrl+O", "fileOpen"},
          {"&Save\tCtrl+S", "fileSave"},
          {"Save &As...\tCtrl+Shift+S", "fileSaveAs"},
          {"", ""},
          {"E&xit\tCtrl+Q", "quit"}}},
        {"&Edit",
         {{"&Undo\tCtrl+Z", "undo"},
          {"&Redo\tCtrl+Y", "redo"},
          {"", ""},
          {"Cu&t\tCtrl+X", "cut"},
          {"&Copy\tCtrl+C", "copy"},
          {"&Paste\tCtrl+V", "paste"},
          {"Paste &Values", "pasteValues"},
          {"Paste &Formats", "pasteFormats"},
          {"", ""},
          {"Clear &Contents", "clearContents"},
          {"Clear For&mats", "clearFormats"},
          {"Clear A&ll", "clearAll"},
          {"", ""},
          {"Select &All\tCtrl+A", "selectAll"},
          {"&Find and Replace...\tCtrl+F", "find"},
          {"&Go To...\tCtrl+G", "goTo"}}},
        {"&View",
         {{"Zoom &In\tCtrl+=", "zoomIn"},
          {"Zoom &Out\tCtrl+-", "zoomOut"},
          {"&Actual Size\tCtrl+0", "zoomReset"},
          {"", ""},
          {"&Grid Lines", "toggleGrid"},
          {"&Formulas\tCtrl+`", "toggleFormulas"}}},
        {"&Insert",
         {{"&Rows", "insertRows"},
          {"&Columns", "insertCols"},
          {"&Sheet", "insertSheet"},
          {"", ""},
          {"&Function...\tShift+F3", "insertFunction"},
          {"&AutoSum\tAlt+=", "autoSum"},
          {"&Name...", "defineName"}}},
        {"F&ormat",
         {{"&Bold\tCtrl+B", "toggleBold"},
          {"&Italic\tCtrl+I", "toggleItalic"},
          {"&Underline\tCtrl+U", "toggleUline"},
          {"&Strikethrough", "toggleStrike"},
          {"", ""},
          {"Align &Left\tCtrl+L", "alignLeft"},
          {"Align Ce&nter\tCtrl+E", "alignCenter"},
          {"Align &Right\tCtrl+R", "alignRight"},
          {"&Justify\tCtrl+J", "alignJustify"},
          {"&Wrap Text", "toggleWrap"},
          {"", ""},
          {"&Increase Indent", "marginIncrease"},
          {"&Decrease Indent", "marginDecrease"},
          {"", ""},
          {"&Custom Number Format...", "customFormat"},
          {"&Merge Cells", "mergeCells"},
          {"Outline &Border", "borderOutline"},
          {"Remove B&order", "borderNone"},
          {"", ""},
          {"Column &Width...", "columnWidth"},
          {"Row &Height...", "rowHeight"},
          {"AutoFit Col&umns", "autofitCols"},
          {"AutoFit Ro&ws", "autofitRows"},
          {"Hide Columns", "hideCols"},
          {"Unhide Columns", "unhideCols"},
          {"Hide Rows", "hideRows"},
          {"Unhide Rows", "unhideRows"},
          {"", ""},
          {"Rename S&heet...", "renameSheet"},
          {"Delete Sheet", "deleteSheet"},
          {"Move Sheet Left", "sheetLeft"},
          {"Move Sheet Right", "sheetRight"}}},
        {"&Data",
         {{"Sort &Ascending", "sortAscending"},
          {"Sort &Descending", "sortDescending"},
          {"", ""},
          {"Delete &Rows", "deleteRows"},
          {"Delete &Columns", "deleteCols"},
          {"", ""},
          {"Re&calculate\tF9", "recalculate"}}},
        {"&Help", {{"&About Isaac", "about"}}},
    };
    return groups;
}

// Report whether a command works on the text of an open editor.
bool edits_text(std::string_view command) {
    static const std::set<std::string_view> names{
        "cut",  "copy",     "paste",   "undo",           "autoSum",
        "goTo", "zoomIn",   "zoomOut", "zoomReset",      "insertFunction",
        "find", "fileSave", "about",   "toggleFormulas", "toggleGrid"};
    return names.count(command) > 0;
}

} // namespace

void main_window::build_menu() {
    for (const menu_group &group : menu_groups()) {
        native::menu_items_proxy items;
        for (const menu_row &row : group.rows) {
            if (row.label[0] == '\0') {
                items << native::menu_separator;
                continue;
            }
            menu_commands_.emplace_back(row.command);
            items << std::make_pair(static_cast<int>(menu_commands_.size()),
                                    std::string(row.label));
        }
        menu << group.title << items;
    }
}

void main_window::run(std::string_view command) {
    message_.clear();
    view::grid_view &grid = canvas_.grid();
    if (grid.editing() && !edits_text(command)) {
        grid.commit();
        if (grid.editing()) {
            canvas_.refresh();
            return;
        }
    }
    if (run_window_command(command))
        return;
    if (view::run_command(grid, command)) {
        canvas_.refresh();
        canvas_.focus();
    }
}

bool main_window::run_window_command(std::string_view command) {
    view::grid_view &grid = canvas_.grid();
    if (command == "fileNew") {
        new_document();
    } else if (command == "fileOpen") {
        open_requested();
    } else if (command == "fileSave") {
        save();
    } else if (command == "fileSaveAs") {
        save_as_requested();
    } else if (command == "quit") {
        request_close();
    } else if (command == "cut") {
        if (!grid.editing())
            write_clipboard(grid.cut());
        canvas_.refresh();
    } else if (command == "copy") {
        if (!grid.editing())
            write_clipboard(grid.copy());
        canvas_.refresh();
    } else if (command == "paste") {
        paste();
    } else if (command == "pasteValues") {
        grid.paste_special({true, false, true, false});
        canvas_.refresh();
    } else if (command == "pasteFormats") {
        grid.paste_special({false, true, false, false});
        canvas_.refresh();
    } else if (command == "autoSum") {
        grid.auto_sum();
        canvas_.refresh();
    } else if (command == "find") {
        show_find();
    } else if (command == "goTo") {
        show_goto();
    } else if (command == "zoomIn") {
        set_zoom(grid.zoom() * 1.25);
    } else if (command == "zoomOut") {
        set_zoom(grid.zoom() / 1.25);
    } else if (command == "zoomReset") {
        set_zoom(1.0);
    } else if (command == "renameSheet") {
        show_rename();
    } else if (command == "columnWidth") {
        show_size(true);
    } else if (command == "rowHeight") {
        show_size(false);
    } else if (command == "customFormat") {
        show_custom_format();
    } else if (command == "defineName") {
        show_define_name();
    } else if (command == "insertFunction") {
        show_function();
    } else if (command == "about") {
        show_about();
    } else {
        return false;
    }
    return true;
}

bool main_window::confirm_discard() {
    if (!canvas_.grid().commands().modified())
        return true;
    switch (native::message_box::show(
        *this, "Save changes to this workbook?", "Isaac",
        native::message_box_buttons::yes_no_cancel,
        native::message_box_icon::question)) {
    case native::message_box_result::yes:
        return save();
    case native::message_box_result::no:
        return true;
    default:
        return false;
    }
}

void main_window::request_close() {
    if (confirm_discard())
        native::app_wnd::request_close();
}

void main_window::new_document() {
    if (!confirm_discard())
        return;
    auto fresh = std::make_unique<book::workbook>();
    impexp::new_workbook(*fresh);
    canvas_.set_path({});
    canvas_.open(std::move(fresh));
}

void main_window::open_requested() {
    if (!confirm_discard())
        return;
    open_dialog_.create();
    open_dialog_.show();
}

void main_window::load(const std::filesystem::path &path) {
    auto source = std::make_unique<book::workbook>();
    const impexp::import_result result = impexp::load_file(path, *source);
    if (!result.ok) {
        native::message_box::show(*this, result.error, "Could not open",
                                  native::message_box_buttons::ok,
                                  native::message_box_icon::error);
        return;
    }
    const impexp::file_format *format = impexp::format_for(path);
    canvas_.set_path(format == impexp::file_formats()[0].get()
                         ? path
                         : std::filesystem::path{});
    canvas_.open(std::move(source));
    if (!result.warnings.empty())
        set_message(result.warnings.front());
}

bool main_window::save() {
    canvas_.grid().commit();
    const std::filesystem::path path = canvas_.path();
    if (path.empty()) {
        save_as_requested();
        return false;
    }
    return save_to(path);
}

bool main_window::save_to(const std::filesystem::path &path) {
    canvas_.grid().store_selection();
    std::string error;
    if (!impexp::save_file(path, canvas_.workbook(), error)) {
        native::message_box::show(*this, error, "Could not save",
                                  native::message_box_buttons::ok,
                                  native::message_box_icon::error);
        return false;
    }
    // CSV and HTML are exports: the workbook stays what it was.
    if (impexp::format_for(path) == impexp::file_formats()[0].get()) {
        canvas_.set_path(path);
        canvas_.grid().commands().mark_saved();
    }
    set_message("Saved " + path.filename().string());
    return true;
}

void main_window::save_as_requested() {
    const std::filesystem::path &path = canvas_.path();
    save_dialog_.set_suggested_name(
        path.empty() ? "Untitled.gnumeric" : path.filename().string());
    save_dialog_.create();
    save_dialog_.show();
}

void main_window::write_clipboard(const std::string &text) {
    if (text.empty())
        return;
    native::clipboard board = native::clipboard::open_write();
    board.write_text(text);
    board.commit();
}

std::string main_window::read_clipboard() {
    const native::clipboard board = native::clipboard::open_read();
    return board.has(native::clipboard_format::text) ? board.read_text()
                                                     : std::string();
}

void main_window::paste() {
    view::grid_view &grid = canvas_.grid();
    const std::string text = read_clipboard();
    if (grid.editing())
        grid.insert_text(text);
    else
        grid.paste(text);
    canvas_.refresh();
}

void main_window::set_zoom(double zoom) {
    canvas_.grid().set_zoom(zoom);
    canvas_.refresh();
}

void main_window::show_find() {
    if (!find_.get_created())
        find_.create();
    find_.show();
}

void main_window::show_about() {
    native::message_box::show(
        *this,
        "Isaac " ISAAC_VERSION "\n\n"
        "A spreadsheet rebuilt from Gnumeric on the Native UI library.\n"
        "Licensed under the GNU GPL, version 2 or later.",
        "About Isaac", native::message_box_buttons::ok,
        native::message_box_icon::information);
}

void main_window::show_goto() {
    input_.ask("Go To", "Cell, range or name:",
               canvas_.grid().position_name(),
               [this](const std::string &text) { go_to(text); });
}

void main_window::go_to(const std::string &target) {
    view::grid_view &grid = canvas_.grid();
    const expr::parse_pos where{&grid.active(), grid.cursor(),
                                &grid.workbook()};
    const expr::parse_result parsed = expr::parse_formula(target, where);
    const expr::eval_pos at{&grid.active(), grid.cursor(), &grid.workbook(),
                            false};
    const auto cells =
        parsed.tree ? parsed.tree->as_area(at) : std::optional<val::area>{};
    if (!cells) {
        set_message("Not a place: " + target);
        return;
    }
    for (int i = 0; i < grid.workbook().sheet_count(); ++i)
        if (&grid.workbook().sheet_at(i) == cells->sheet)
            grid.set_active(i);
    grid.move_to(cells->range.start, false);
    grid.move_to(cells->range.end, true);
    grid.move_to(cells->range.start, true);
    grid.reveal_cursor();
    canvas_.refresh();
    canvas_.focus();
}

void main_window::show_rename() {
    view::grid_view &grid = canvas_.grid();
    input_.ask("Rename Sheet", "Sheet name:", grid.active().name(),
               [this](const std::string &text) {
                   if (!canvas_.grid().rename_sheet(text))
                       set_message("That name is empty or already used");
                   canvas_.refresh();
               });
}

void main_window::show_size(bool columns) {
    view::grid_view &grid = canvas_.grid();
    const val::cell_range range = grid.current_range();
    const double current = columns ? grid.active().cols().points(grid.cursor().col)
                                   : grid.active().rows().points(grid.cursor().row);
    input_.ask(columns ? "Column Width" : "Row Height", "Size in points:",
               std::to_string(current),
               [this, columns, range](const std::string &text) {
                   const double points = std::atof(text.c_str());
                   if (points <= 0 || points > 1000) {
                       set_message("Not a size: " + text);
                       return;
                   }
                   view::grid_view &view = canvas_.grid();
                   if (columns)
                       view.set_col_width(range.start.col, range.end.col,
                                          points);
                   else
                       view.set_row_height(range.start.row, range.end.row,
                                           points);
                   canvas_.refresh();
               });
}

void main_window::show_custom_format() {
    input_.ask("Custom Number Format", "Format:",
               canvas_.grid().cursor_style().format,
               [this](const std::string &text) {
                   book::style_patch patch;
                   patch.format = text;
                   canvas_.grid().format(patch, "Set number format");
                   canvas_.refresh();
               });
}

void main_window::show_define_name() {
    view::grid_view &grid = canvas_.grid();
    const std::string target =
        "$" + val::col_name(grid.current_range().start.col) + "$" +
        val::row_name(grid.current_range().start.row);
    input_.ask("Define Name", "name=reference, for example tax=$B$1:",
               "=" + target, [this](const std::string &text) {
                   view::grid_view &view = canvas_.grid();
                   const auto equals = text.find('=');
                   if (equals == std::string::npos || equals == 0) {
                       set_message("Write name=reference");
                       return;
                   }
                   const expr::parse_pos where{&view.active(), view.cursor(),
                                               &view.workbook()};
                   expr::parse_result parsed = expr::parse_formula(
                       std::string_view(text).substr(equals + 1), where);
                   if (!parsed.tree) {
                       set_message(parsed.error);
                       return;
                   }
                   view.commands().run(cmd::define_name(
                       text.substr(0, equals), std::move(parsed.tree), nullptr,
                       view.cursor()));
                   canvas_.refresh();
               });
}

void main_window::show_function() {
    function_.ask([this](const std::string &name) {
        view::grid_view &grid = canvas_.grid();
        if (grid.editing())
            grid.insert_text(name + "(");
        else
            grid.begin_typing("=" + name + "(");
        canvas_.refresh();
        canvas_.focus();
    });
}

} // namespace ui
