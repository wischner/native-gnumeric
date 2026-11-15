//
// Search commands forwarded to the sheet.
//
// Derived from Gnumeric src/dialogs/dialog-search.c,
// Copyright (C) 2001-2009 Morten Welinder.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include "find_window.h"
#include "form_layout.h"

#include "cmd/edits.h"
#include "util/text.h"

namespace ui {

find_window::find_window(native::app_wnd &owner, grid_canvas &target,
                         std::function<void(const std::string &)> report)
    : native::modeless_wnd(owner, "Find and Replace", 160, 160, 1, 1),
      target_(target), report_(std::move(report)),
      needle_("", native::text_edit_mode::single_line),
      replacement_("", native::text_edit_mode::single_line),
      match_case_("Match case"), whole_cell_("Whole cell"),
      find_button_("Find"), replace_button_("Replace"), all_button_("Replace All") {
    on_wnd_create.connect([this] {
        unsigned order = 0;
        for (native::wnd *child : std::initializer_list<native::wnd *>{
                 &needle_, &replacement_, &match_case_, &whole_cell_,
                 &find_button_, &replace_button_, &all_button_})
            attach_form_control(*this, *child, ++order);
        layout();
        return true;
    });
    on_wnd_resize.connect([this](native::size) {
        if (all_button_.get_created()) layout();
        return true;
    });
    on_wnd_paint.connect([this](native::wnd_paint_event event) {
        const auto m = measure_form(*this);
        const auto area = form_area(*this, m);
        paint_form_label(event, "Find:",
                         native::rect(area.x1(), area.y1(), area.w(), m.line));
        paint_form_label(event, "Replace with:",
                         native::rect(area.x1(), area.y1() + m.line + m.field +
                                      2 * m.gap, area.w(), m.line));
        return true;
    });
    find_button_.on_click.connect([this] {
        find();
        return true;
    });
    replace_button_.on_click.connect([this] {
        replace();
        return true;
    });
    all_button_.on_click.connect([this] {
        replace_all();
        return true;
    });
}

void find_window::open() {
    if (!get_created()) create();
    center_form(*this);
    show();
    needle_.focus();
}

void find_window::layout() {
    const auto m = measure_form(*this);
    fit_form(*this, 3 * m.button_width + 2 * m.gap + 2 * m.margin,
             2 * m.margin + 2 * m.line + 2 * m.field + m.check +
             m.button + 7 * m.gap);
    native::grid_layout_manager grid;
    grid << native::column(native::star())
         << native::column(native::pixels(m.gap))
         << native::column(native::star())
         << native::column(native::pixels(m.gap))
         << native::column(native::star());
    for (int height : {m.line, m.gap, m.field, m.gap, m.line,
                       m.gap, m.field, m.gap, m.check, 2 * m.gap})
        grid << native::row(native::pixels(height));
    grid << native::row(native::star()) << native::row(native::pixels(m.button));
    grid << native::cell(needle_, 2, 0, 1, 5)
         << native::cell(replacement_, 6, 0, 1, 5)
         << native::cell(find_button_, 11, 0)
         << native::cell(replace_button_, 11, 2)
         << native::cell(all_button_, 11, 4);
    auto checks = std::make_unique<native::grid_layout_manager>();
    *checks << native::row(native::star()) << native::column(native::star())
            << native::column(native::pixels(m.gap))
            << native::column(native::star())
            << native::cell(match_case_, 0, 0) << native::cell(whole_cell_, 0, 2);
    grid << native::child_grid(std::move(checks), 8, 0, 1, 5);
    grid.relayout(this, form_area(*this, m));
}

book::search_options find_window::options() const {
    book::search_options result;
    result.match_case = match_case_.get_checked();
    result.whole_cell = whole_cell_.get_checked();
    return result;
}

void find_window::find() {
    view::grid_view &grid = target_.grid();
    const auto hit = book::find_text(grid.active(), grid.cursor(),
                                     needle_.get_text(), options());
    if (!hit) {
        report_("Not found: " + needle_.get_text());
        return;
    }
    grid.move_to(hit->pos, false);
    grid.reveal_cursor();
    target_.refresh();
}

void find_window::replace() {
    view::grid_view &grid = target_.grid();
    const auto hit = book::find_text(grid.active(), grid.cursor(),
                                     needle_.get_text(), options());
    if (!hit) {
        report_("Not found: " + needle_.get_text());
        return;
    }
    // The hit becomes a single cell's replacement command.
    grid.move_to(hit->pos, false);
    book::search_options whole = options();
    const std::string before = grid.cursor_entry();
    std::string after = before;
    const std::string &needle = needle_.get_text();
    if (whole.whole_cell) {
        after = replacement_.get_text();
    } else {
        const std::string subject = whole.match_case ? before : util::to_lower(before);
        const std::string query = whole.match_case ? needle : util::to_lower(needle);
        after.clear();
        std::size_t copied = 0;
        for (std::size_t at = subject.find(query); at != std::string::npos;
             at = subject.find(query, copied)) {
            after.append(before, copied, at - copied);
            after += replacement_.get_text();
            copied = at + query.size();
        }
        after.append(before, copied, std::string::npos);
    }
    if (after != before)
        grid.commands().run(cmd::set_entry(grid.active(), grid.cursor(), after));
    find();
}

void find_window::replace_all() {
    view::grid_view &grid = target_.grid();
    grid.commands().run(cmd::replace_all(grid.active(), needle_.get_text(),
                                         replacement_.get_text(), options()));
    report_("Replaced in the sheet");
    target_.refresh();
}

} // namespace ui
