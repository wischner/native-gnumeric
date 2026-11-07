//
// Search commands forwarded to the sheet.
//
// Derived from Gnumeric src/dialogs/dialog-search.c,
// Copyright (C) 2001-2009 Morten Welinder.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include "find_window.h"

#include "cmd/edits.h"

namespace ui {

find_window::find_window(native::app_wnd &owner, grid_canvas &target,
                         std::function<void(const std::string &)> report)
    : native::modeless_wnd(owner, "Find and Replace", 160, 160, 420, 150),
      target_(target), report_(std::move(report)),
      needle_("", native::text_edit_mode::single_line, 12, 12, 280, 26),
      replacement_("", native::text_edit_mode::single_line, 12, 44, 280, 26),
      match_case_("Match case", 12, 80, 130, 24),
      whole_cell_("Whole cell", 150, 80, 130, 24),
      find_button_("Find", 304, 12, 104, 28),
      replace_button_("Replace", 304, 46, 104, 28),
      all_button_("Replace All", 304, 80, 104, 28) {
    on_wnd_create.connect([this] {
        attach(needle_);
        attach(replacement_);
        attach(match_case_);
        attach(whole_cell_);
        attach(find_button_);
        attach(replace_button_);
        attach(all_button_);
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

void find_window::attach(native::wnd &child) {
    child.set_parent(this);
    child.create();
    child.show();
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
        for (std::size_t at = after.find(needle); at != std::string::npos;
             at = after.find(needle, at + replacement_.get_text().size()))
            after.replace(at, needle.size(), replacement_.get_text());
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
