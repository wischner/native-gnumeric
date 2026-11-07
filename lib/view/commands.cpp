//
// Named commands. The menus, the toolbars and the keys all reach the
// view through these names, as AbiWord's edit methods and Gnumeric's
// action tables do.
//
// Derived from Gnumeric src/wbc-gtk-actions.c and src/cmd-edit.c,
// Copyright (C) 1998-2003 Miguel de Icaza, Jody Goldberg, Morten Welinder.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include <algorithm>
#include <map>

#include "view/grid_view.h"

namespace view {
namespace {

using action = void (*)(grid_view &);

// Change one style field by a patch built in place.
template <typename build_t>
void patch(grid_view &target, const char *name, build_t build) {
    book::style_patch change;
    build(change);
    target.format(change, name);
}

// Apply a number format to the selection.
void number_format(grid_view &target, const char *format) {
    patch(target, "Set number format",
          [&](book::style_patch &p) { p.format = format; });
}

// Set horizontal alignment.
void align(grid_view &target, book::h_align value) {
    const book::h_align current = target.cursor_style().horizontal;
    patch(target, "Align", [&](book::style_patch &p) {
        p.horizontal = current == value ? book::h_align::general : value;
    });
}

// Put the same border on the outside of the selection.
void outline(grid_view &target, book::border_line line) {
    const cell_range range = target.current_range();
    sheet &item = target.active();
    const auto side = [&](book::border_side which, const cell_range &part) {
        book::style_patch change;
        change.borders[static_cast<std::size_t>(which)] =
            book::border_edge{line, {0, 0, 0}};
        target.commands().run(
            cmd::format_cells(item, {part}, change, "Set borders"));
    };
    side(book::border_side::top, {range.start, {range.end.col, range.start.row}});
    side(book::border_side::bottom,
         {{range.start.col, range.end.row}, range.end});
    side(book::border_side::left, {range.start, {range.start.col, range.end.row}});
    side(book::border_side::right,
         {{range.end.col, range.start.row}, range.end});
}

// Change the indent of the selection.
void indent(grid_view &target, int change) {
    const int level = std::max(0, target.cursor_style().indent + change);
    patch(target, "Indent",
          [&](book::style_patch &p) { p.indent = level; });
}

// Change the decimals of the cursor's format.
void decimals(grid_view &target, int change) {
    std::string format = target.cursor_style().format;
    if (format == "General")
        format = "0";
    const auto point = format.find('.');
    if (change > 0) {
        if (point == std::string::npos) {
            const auto last = format.find_last_of('0');
            if (last != std::string::npos)
                format.insert(last + 1, ".0");
        } else {
            format.insert(point + 1, "0");
        }
    } else if (point != std::string::npos) {
        format.erase(point + 1, 1);
        if (point + 1 >= format.size() || format[point + 1] != '0')
            format.erase(point, 1);
    }
    number_format(target, format.c_str());
}

const std::map<std::string, action, std::less<>> &commands() {
    static const std::map<std::string, action, std::less<>> table = {
        {"undo", [](grid_view &v) { v.undo(); }},
        {"redo", [](grid_view &v) { v.redo(); }},
        {"selectAll", [](grid_view &v) { v.select_all(); }},
        {"clearContents",
         [](grid_view &v) { v.clear_selection(cmd::clear_kind::contents); }},
        {"clearFormats",
         [](grid_view &v) { v.clear_selection(cmd::clear_kind::formats); }},
        {"clearAll",
         [](grid_view &v) { v.clear_selection(cmd::clear_kind::all); }},
        {"insertRows", [](grid_view &v) { v.insert_rows(); }},
        {"insertCols", [](grid_view &v) { v.insert_cols(); }},
        {"deleteRows", [](grid_view &v) { v.delete_rows(); }},
        {"deleteCols", [](grid_view &v) { v.delete_cols(); }},
        {"insertSheet", [](grid_view &v) { v.add_sheet(); }},
        {"deleteSheet", [](grid_view &v) { v.remove_sheet(); }},
        {"sheetLeft", [](grid_view &v) { v.move_sheet(-1); }},
        {"sheetRight", [](grid_view &v) { v.move_sheet(1); }},
        {"sheetNext",
         [](grid_view &v) { v.set_active(v.active_index() + 1); }},
        {"sheetPrevious",
         [](grid_view &v) { v.set_active(v.active_index() - 1); }},
        {"toggleBold",
         [](grid_view &v) {
             const bool next = !v.cursor_style().bold;
             patch(v, "Bold", [&](book::style_patch &p) { p.bold = next; });
         }},
        {"toggleItalic",
         [](grid_view &v) {
             const bool next = !v.cursor_style().italic;
             patch(v, "Italic",
                   [&](book::style_patch &p) { p.italic = next; });
         }},
        {"toggleUline",
         [](grid_view &v) {
             const int next = v.cursor_style().underline ? 0 : 1;
             patch(v, "Underline",
                   [&](book::style_patch &p) { p.underline = next; });
         }},
        {"toggleStrike",
         [](grid_view &v) {
             const bool next = !v.cursor_style().strike;
             patch(v, "Strikethrough",
                   [&](book::style_patch &p) { p.strike = next; });
         }},
        {"toggleWrap",
         [](grid_view &v) {
             const bool next = !v.cursor_style().wrap;
             patch(v, "Wrap text",
                   [&](book::style_patch &p) { p.wrap = next; });
         }},
        {"alignLeft", [](grid_view &v) { align(v, book::h_align::left); }},
        {"alignCenter", [](grid_view &v) { align(v, book::h_align::center); }},
        {"alignRight", [](grid_view &v) { align(v, book::h_align::right); }},
        {"alignJustify",
         [](grid_view &v) { align(v, book::h_align::justify); }},
        {"marginIncrease", [](grid_view &v) { indent(v, 1); }},
        {"marginDecrease", [](grid_view &v) { indent(v, -1); }},
        {"formatGeneral",
         [](grid_view &v) { number_format(v, "General"); }},
        {"formatNumber", [](grid_view &v) { number_format(v, "#,##0.00"); }},
        {"formatCurrency",
         [](grid_view &v) { number_format(v, "$#,##0.00"); }},
        {"formatPercent", [](grid_view &v) { number_format(v, "0%"); }},
        {"formatDate", [](grid_view &v) { number_format(v, "m/d/yyyy"); }},
        {"formatTime", [](grid_view &v) { number_format(v, "h:mm"); }},
        {"formatScientific",
         [](grid_view &v) { number_format(v, "0.00E+00"); }},
        {"formatFraction", [](grid_view &v) { number_format(v, "# ?/?"); }},
        {"formatText", [](grid_view &v) { number_format(v, "@"); }},
        {"decimalsMore", [](grid_view &v) { decimals(v, 1); }},
        {"decimalsLess", [](grid_view &v) { decimals(v, -1); }},
        {"mergeCells", [](grid_view &v) { v.toggle_merge(); }},
        {"borderOutline",
         [](grid_view &v) { outline(v, book::border_line::thin); }},
        {"borderNone",
         [](grid_view &v) { outline(v, book::border_line::none); }},
        {"hideCols", [](grid_view &v) { v.set_hidden(true, true); }},
        {"unhideCols", [](grid_view &v) { v.set_hidden(true, false); }},
        {"hideRows", [](grid_view &v) { v.set_hidden(false, true); }},
        {"unhideRows", [](grid_view &v) { v.set_hidden(false, false); }},
        {"autofitCols",
         [](grid_view &v) {
             const cell_range range = v.current_range();
             v.autofit_cols(range.start.col, range.end.col);
         }},
        {"autofitRows",
         [](grid_view &v) {
             const cell_range range = v.current_range();
             v.autofit_rows(range.start.row, range.end.row);
         }},
        {"sortAscending", [](grid_view &v) { v.sort_selection(true); }},
        {"sortDescending", [](grid_view &v) { v.sort_selection(false); }},
        {"recalculate",
         [](grid_view &v) {
             v.workbook().queue_all();
             v.workbook().recalculate();
         }},
        {"toggleGrid",
         [](grid_view &v) { v.active().show_grid = !v.active().show_grid; }},
        {"toggleFormulas",
         [](grid_view &v) {
             v.active().show_formulas = !v.active().show_formulas;
         }},
    };
    return table;
}

} // namespace

bool run_command(grid_view &target, std::string_view command) {
    const auto found = commands().find(command);
    if (found == commands().end())
        return false;
    found->second(target);
    return true;
}

} // namespace view
