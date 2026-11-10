//
// Window construction and state display. After every change the
// controls are re-read from the view, so toolbar, format bar and status
// bar cannot disagree with the workbook.
//
// Derived from Gnumeric src/wbc-gtk.c,
// Copyright (C) 2000-2006 Jody Goldberg, Morten Welinder and others.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include "main_window.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>

#include "numfmt/format.h"
#include "tool_icons.h"

namespace ui {
namespace {

constexpr int header_height = 22;
constexpr int header_width = 44;

// As long as any window: a hosted control is clipped at the strip's end.
constexpr int formula_length = 4096;

// A label and its stored color.
struct color_choice {
    const char *label;
    util::color value;
};

constexpr std::array<color_choice, 8> colors{{{"Black", {0, 0, 0}},
                                              {"Gray", {128, 128, 128}},
                                              {"Red", {192, 57, 43}},
                                              {"Orange", {211, 84, 0}},
                                              {"Green", {30, 132, 73}},
                                              {"Blue", {31, 78, 158}},
                                              {"Purple", {118, 68, 138}},
                                              {"White", {255, 255, 255}}}};

constexpr std::array<int, 16> font_sizes{8,  9,  10, 11, 12, 14, 16, 18,
                                         20, 22, 24, 28, 32, 36, 48, 72};

constexpr std::array<int, 6> zoom_levels{50, 75, 100, 125, 150, 200};

// One number format offered by the format bar.
struct format_choice {
    const char *label;
    const char *command;
    numfmt::format_family family;
};

constexpr std::array<format_choice, 9> number_formats{{
    {"General", "formatGeneral", numfmt::format_family::general},
    {"Number", "formatNumber", numfmt::format_family::number},
    {"Currency", "formatCurrency", numfmt::format_family::currency},
    {"Percent", "formatPercent", numfmt::format_family::percent},
    {"Scientific", "formatScientific", numfmt::format_family::scientific},
    {"Date", "formatDate", numfmt::format_family::date},
    {"Time", "formatTime", numfmt::format_family::time},
    {"Fraction", "formatFraction", numfmt::format_family::fraction},
    {"Text", "formatText", numfmt::format_family::text},
}};

// One toolbar entry; an empty command is a separator.
struct tool_entry {
    const char *command;
    native::tool_kind kind;
    const char *group;
};

constexpr std::array<tool_entry, 15> tools{{
    {"fileNew", native::tool_kind::button, ""},
    {"fileOpen", native::tool_kind::button, ""},
    {"fileSave", native::tool_kind::button, ""},
    {"", native::tool_kind::separator, ""},
    {"undo", native::tool_kind::button, ""},
    {"redo", native::tool_kind::button, ""},
    {"", native::tool_kind::separator, ""},
    {"cut", native::tool_kind::button, ""},
    {"copy", native::tool_kind::button, ""},
    {"paste", native::tool_kind::button, ""},
    {"find", native::tool_kind::button, ""},
    {"", native::tool_kind::separator, ""},
    {"autoSum", native::tool_kind::button, ""},
    {"sortAscending", native::tool_kind::button, ""},
    {"sortDescending", native::tool_kind::button, ""},
}};

// The format bar's plain tools, after its hosted combo boxes.
constexpr std::array<tool_entry, 17> format_tools{{
    {"", native::tool_kind::separator, ""},
    {"toggleBold", native::tool_kind::toggle, ""},
    {"toggleItalic", native::tool_kind::toggle, ""},
    {"toggleUline", native::tool_kind::toggle, ""},
    {"toggleStrike", native::tool_kind::toggle, ""},
    {"", native::tool_kind::separator, ""},
    {"alignLeft", native::tool_kind::exclusive, "align"},
    {"alignCenter", native::tool_kind::exclusive, "align"},
    {"alignRight", native::tool_kind::exclusive, "align"},
    {"alignJustify", native::tool_kind::exclusive, "align"},
    {"", native::tool_kind::separator, ""},
    {"mergeCells", native::tool_kind::button, ""},
    {"borderOutline", native::tool_kind::button, ""},
    {"formatPercent", native::tool_kind::button, ""},
    {"formatCurrency", native::tool_kind::button, ""},
    {"decimalsMore", native::tool_kind::button, ""},
    {"decimalsLess", native::tool_kind::button, ""},
}};

// Return a tool item for one table entry.
native::tool_item make_tool(const tool_entry &entry) {
    native::tool_item item;
    item.id = entry.command;
    item.kind = entry.kind;
    item.group = entry.group;
    item.icon = tool_icon(entry.command);
    return item;
}

// Return a hosted control item.
native::tool_item make_control(const char *id, native::wnd &control,
                               int length, bool fill_width = false) {
    native::tool_item item;
    item.id = id;
    item.kind = native::tool_kind::control;
    item.control = &control;
    item.length = length;
    item.control_fill_width = fill_width;
    return item;
}

// These Native setters repaint even when the value is unchanged. Keep
// ordinary typing from repainting the surrounding controls.
void update_checked(native::toolbar &bar, const std::string &id, bool value) {
    if (bar.get_checked(id) != value)
        bar.set_checked(id, value);
}

void update_enabled(native::toolbar &bar, const std::string &id, bool value) {
    const auto &items = bar.get_items();
    const auto item = std::ranges::find(items, id, &native::tool_item::id);
    if (item != items.end() && item->enabled != value)
        bar.set_enabled(id, value);
}

// Return the items of the zoom box.
std::vector<std::string> zoom_labels() {
    std::vector<std::string> result;
    for (const int level : zoom_levels)
        result.push_back(std::to_string(level) + "%");
    return result;
}

// Return the items of the number format box.
std::vector<std::string> format_labels() {
    std::vector<std::string> result;
    for (const format_choice &choice : number_formats)
        result.emplace_back(choice.label);
    return result;
}

} // namespace

main_window::main_window(const app::options &settings)
    : native::app_wnd("Isaac", 60, 40, 1000, 760), settings_(settings),
      number_box_(format_labels(), native::combo_box_style::drop_down_list),
      font_box_(fonts_.families(), native::combo_box_style::drop_down_list),
      size_box_({}, native::combo_box_style::drop_down_list),
      color_box_({}, native::combo_box_style::drop_down_list),
      fill_box_({}, native::combo_box_style::drop_down_list),
      zoom_box_(zoom_labels(), native::combo_box_style::drop_down_list),
      canvas_(fonts_), formula_(canvas_),
      toolbar_(*this), format_bar_(*this), formula_strip_(*this),
      sheet_bar_(canvas_, native::window_edge::bottom),
      col_header_(canvas_, native::window_edge::top, header_height),
      row_header_(canvas_, native::window_edge::left, header_width),
      status_(*this, canvas_), open_dialog_(*this, "Open"),
      save_dialog_(*this, "Save As"),
      find_(*this, canvas_,
            [this](const std::string &text) { set_message(text); }),
      input_(*this), function_(*this) {
    canvas_.set_parent(this);
    set_scroll_content(&canvas_);
    // Joined fields share one divider; the toolbar supplies horizontal rules.
    for (auto *box : {&number_box_, &font_box_, &size_box_, &color_box_,
                      &fill_box_, &zoom_box_})
        box->set_border_sides(native::border_sides::right);
    build_menu();
    build_toolbar();
    build_format_bar();
    build_formula_strip();
    tabs_.set_selection_only(true).set_page_frame_visible(false)
        .set_tab_placement(native::tab_placement::bottom);
    sheet_bar_.set_border_sides(native::border_sides::none);
    sheet_bar_.add_item(make_control("sheetTabs", tabs_, formula_length));
    canvas_.set_change_handler([this] { update_state(); });
    canvas_.set_command_handler(
        [this](std::string_view command) { run(command); });
    formula_.set_name_handler([this] { show_goto(); });
    tabs_.on_selection_change.connect([this](int index) {
        canvas_.grid().set_active(index);
        canvas_.refresh();
        canvas_.focus();
        return true;
    });
    tabs_.on_activate.connect([this](int) {
        show_rename();
        return true;
    });

    for (const auto &format : impexp::file_formats()) {
        const std::string pattern = "*." + std::string(format->extension());
        if (format->can_read())
            open_dialog_.add_filter({std::string(format->name()), {pattern}});
        if (format->can_write())
            save_dialog_.add_filter({std::string(format->name()), {pattern}});
    }
    open_dialog_.add_filter({"All files", {"*"}});
    save_dialog_.set_default_extension("gnumeric");
    open_dialog_.on_modal_close.connect([this](native::dialog_result result) {
        if (result == native::dialog_result::accepted)
            load(open_dialog_.get_path());
        return true;
    });
    save_dialog_.on_modal_close.connect([this](native::dialog_result result) {
        if (result == native::dialog_result::accepted)
            save_to(save_dialog_.get_path());
        return true;
    });

    on_wnd_create.connect([this] {
        const auto metrics = native::theme::create(get_gpx())->defaults();
        // Hosted fields keep their native height, including on non-GEM UIs.
        const int extent = std::max(metrics.toolbar_height,
            metrics.text_edit_height + 2 * (metrics.toolbar_padding +
                                            metrics.toolbar_control_padding));
        toolbar_.set_extent(extent);
        format_bar_.set_extent(extent);
        formula_strip_.set_extent(extent);
        for (auto *bar : {&toolbar_, &format_bar_}) {
            auto items = bar->get_items();
            const int button_width = bar->get_item_bounds(
                bar == &toolbar_ ? "fileNew" : "toggleBold").w();
            for (auto &item : items)
                if (item.kind == native::tool_kind::separator)
                    item.length = button_width;
            bar->set_items(std::move(items));
        }
        status_.set_extent(
            native::theme::create(get_gpx())->get_status_bar_height());
        canvas_.create();
        canvas_.show();
        const int tab_extent = tabs_.get_tab_extent();
        const auto tab_size = tabs_.get_dimensions();
        tabs_.set_dimensions({tab_size.w, static_cast<native::dim>(tab_extent)});
        sheet_bar_.set_extent(tab_extent + 2 * (metrics.toolbar_padding +
                                               metrics.toolbar_control_padding));
        place_children();
        if (settings_.file)
            load(*settings_.file);
        canvas_.refresh();
        canvas_.focus();
        return true;
    });
    on_wnd_resize.connect([this](native::size) {
        place_children();
        return true;
    });
    on_menu.connect([this](int id) {
        const auto index = static_cast<std::size_t>(id - 1);
        if (index < menu_commands_.size())
            run(menu_commands_[index]);
        return true;
    });
}

void main_window::build_toolbar() {
    for (const tool_entry &entry : tools)
        toolbar_.add_item(make_tool(entry));
    toolbar_.add_item(make_tool({"", native::tool_kind::separator, ""}));
    toolbar_.add_item(make_control("zoomBox", zoom_box_, 80, true));
    const auto run_tool = [this](native::tool_command command) {
        run(command.id);
        return true;
    };
    toolbar_.on_command.connect(run_tool);
    format_bar_.on_command.connect(run_tool);
}

void main_window::build_format_bar() {
    format_bar_.add_item(make_control("numberBox", number_box_, 110, true));
    format_bar_.add_item(make_control("fontBox", font_box_, 180, true));
    format_bar_.add_item(make_control("sizeBox", size_box_, 64, true));
    format_bar_.add_item(make_control("colorBox", color_box_, 90, true));
    format_bar_.add_item(make_control("fillBox", fill_box_, 90, true));
    for (const tool_entry &entry : format_tools)
        format_bar_.add_item(make_tool(entry));

    for (const int size : font_sizes)
        size_box_.add_item(std::to_string(size));
    fill_box_.add_item("No fill");
    for (const color_choice &choice : colors) {
        color_box_.add_item(choice.label);
        fill_box_.add_item(choice.label);
    }
    color_box_.set_selected_index(0);
    fill_box_.set_selected_index(0);

    const auto apply = [this](const book::style_patch &patch,
                              const char *name) {
        canvas_.grid().commit();
        canvas_.grid().format(patch, name);
        canvas_.refresh();
        canvas_.focus();
    };
    number_box_.on_selection_change.connect([this](int index) {
        if (updating_ || index < 0)
            return true;
        run(number_formats[static_cast<std::size_t>(index)].command);
        return true;
    });
    font_box_.on_selection_change.connect([this, apply](int index) {
        if (updating_ || index < 0)
            return true;
        book::style_patch patch;
        patch.font_name = font_box_.get_items()[index];
        apply(patch, "Set font");
        return true;
    });
    size_box_.on_selection_change.connect([this, apply](int index) {
        if (updating_ || index < 0)
            return true;
        book::style_patch patch;
        patch.font_size = std::atof(size_box_.get_items()[index].c_str());
        apply(patch, "Set font size");
        return true;
    });
    color_box_.on_selection_change.connect([this, apply](int index) {
        if (updating_ || index < 0)
            return true;
        book::style_patch patch;
        patch.fore = colors[static_cast<std::size_t>(index)].value;
        apply(patch, "Set text color");
        return true;
    });
    fill_box_.on_selection_change.connect([this, apply](int index) {
        if (updating_ || index < 0)
            return true;
        book::style_patch patch;
        patch.pattern = index == 0 ? 0 : 1;
        if (index > 0)
            patch.back = colors[static_cast<std::size_t>(index - 1)].value;
        apply(patch, "Set fill color");
        return true;
    });
    zoom_box_.on_selection_change.connect([this](int index) {
        if (updating_ || index < 0)
            return true;
        set_zoom(zoom_levels[static_cast<std::size_t>(index)] / 100.0);
        return true;
    });
}

void main_window::build_formula_strip() {
    formula_strip_.add_item(
        make_control("formulaBar", formula_, formula_length, true));
}

void main_window::place_children() {
    canvas_.set_bounds(get_client_bounds());
}

void main_window::set_message(const std::string &text) {
    message_ = text;
    update_state();
}

void main_window::update_title() {
    const std::filesystem::path &path = canvas_.path();
    const std::string name =
        path.empty() ? std::string("Untitled") : path.filename().string();
    const bool dirty = canvas_.grid().commands().modified();
    set_title((dirty ? "*" : "") + name + " - Isaac");
}

void main_window::update_tabs() {
    view::grid_view &grid = canvas_.grid();
    std::vector<std::string> names;
    for (int i = 0; i < grid.workbook().sheet_count(); ++i)
        names.push_back(grid.workbook().sheet_at(i).name());
    if (names != shown_tabs_) {
        tabs_.clear_items();
        for (const auto &name : names)
            tabs_.add_item(name);
        shown_tabs_ = std::move(names);
    }
    if (tabs_.get_selected_index() != grid.active_index())
        tabs_.set_selected_index(grid.active_index());
}

void main_window::update_format_controls() {
    const book::style &look = canvas_.grid().cursor_style();
    update_checked(format_bar_, "toggleBold", look.bold);
    update_checked(format_bar_, "toggleItalic", look.italic);
    update_checked(format_bar_, "toggleUline", look.underline != 0);
    update_checked(format_bar_, "toggleStrike", look.strike);
    update_checked(format_bar_, "alignLeft",
                   look.horizontal == book::h_align::left);
    update_checked(format_bar_, "alignCenter",
                   look.horizontal == book::h_align::center);
    update_checked(format_bar_, "alignRight",
                   look.horizontal == book::h_align::right);
    update_checked(format_bar_, "alignJustify",
                   look.horizontal == book::h_align::justify);

    // A workbook may hold a value the box does not offer: list it, so the
    // box shows what the cell asks for rather than going blank.
    const auto select = [](native::combo_box &box, const std::string &text) {
        if (std::ranges::find(box.get_items(), text) == box.get_items().end())
            box.add_item(text);
        const auto &items = box.get_items();
        const auto index = static_cast<int>(
            std::ranges::find(items, text) - items.begin());
        if (box.get_selected_index() != index)
            box.set_selected_index(index);
    };
    select(font_box_, look.font_name);
    char size[16];
    std::snprintf(size, sizeof size, "%g", look.font_size);
    select(size_box_, size);
    const numfmt::format_family family =
        numfmt::number_format::get(look.format)->family();
    const auto format = std::ranges::find(number_formats, family,
                                          &format_choice::family);
    const int index = format == number_formats.end()
                          ? -1
                          : static_cast<int>(format - number_formats.begin());
    if (number_box_.get_selected_index() != index)
        number_box_.set_selected_index(index);
}

void main_window::update_state() {
    updating_ = true;
    view::grid_view &grid = canvas_.grid();
    update_format_controls();
    update_enabled(toolbar_, "undo", grid.commands().can_undo());
    update_enabled(toolbar_, "redo", grid.commands().can_redo());
    const std::string zoom =
        std::to_string(static_cast<int>(std::lround(grid.zoom() * 100))) + "%";
    if (std::ranges::find(zoom_box_.get_items(), zoom) ==
        zoom_box_.get_items().end())
        zoom_box_.add_item(zoom);
    const auto &zooms = zoom_box_.get_items();
    const auto zoom_index =
        static_cast<int>(std::ranges::find(zooms, zoom) - zooms.begin());
    if (zoom_box_.get_selected_index() != zoom_index)
        zoom_box_.set_selected_index(zoom_index);
    update_tabs();
    updating_ = false;

    col_header_.refresh();
    row_header_.refresh();
    formula_.refresh();
    if (!grid.edit_error().empty())
        message_ = grid.edit_error();
    status_.set_parts(
        {{"Sheet " + std::to_string(grid.active_index() + 1) + " of " +
              std::to_string(grid.workbook().sheet_count()),
          130},
         {grid.selection_summary(), 150},
         {zoom, 70},
         {message_, 0}});
    update_title();
}

} // namespace ui
