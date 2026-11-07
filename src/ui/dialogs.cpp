//
// The small dialogs.
//
// Derived from Gnumeric src/dialogs/dialog-function-select.c,
// Copyright (C) 2000-2008 Jody Goldberg, Jukka-Pekka Iivonen, Andreas
// J. Guelzow.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include "dialogs.h"

#include <algorithm>
#include <set>

#include "func/functions.h"

namespace ui {
namespace {

// Return every category of function, sorted.
std::vector<std::string> categories() {
    std::set<std::string> names;
    for (const expr::function_def *item : func::builtin_functions().all())
        names.emplace(item->category);
    return {names.begin(), names.end()};
}

// Paint a dialog's background and one line of text.
void paint_prompt(native::wnd_paint_event event, const std::string &text,
                  const native::rect &area) {
    auto appearance = native::theme::create(event.g);
    event.g.set_ink(appearance->get_button_foreground_color());
    event.g.set_font(native::font_t::stock(native::font_role::control));
    native::text_layout layout;
    layout.vertical = native::text_valign::center;
    event.g.draw_text(text, area, layout);
}

} // namespace

input_window::input_window(native::app_wnd &owner)
    : native::modal_wnd(owner, "Isaac", 200, 200, 380, 124),
      answer_("", native::text_edit_mode::single_line, 12, 40, 356, 26),
      ok_("OK", 180, 80, 88, 28), cancel_("Cancel", 280, 80, 88, 28) {
    on_wnd_create.connect([this] {
        attach(answer_);
        attach(ok_);
        attach(cancel_);
        return true;
    });
    on_wnd_paint.connect([this](native::wnd_paint_event event) {
        paint_prompt(event, prompt_, native::rect(12, 10, 356, 24));
        return true;
    });
    ok_.on_click.connect([this] {
        finish(true);
        return true;
    });
    cancel_.on_click.connect([this] {
        finish(false);
        return true;
    });
}

void input_window::attach(native::wnd &child) {
    child.set_parent(this);
    child.create();
    child.show();
}

void input_window::ask(const std::string &title, const std::string &prompt,
                       const std::string &initial,
                       std::function<void(const std::string &)> accept) {
    prompt_ = prompt;
    accept_ = std::move(accept);
    set_title(title);
    if (!get_created())
        create();
    answer_.set_text(initial);
    center_to_parent();
    show();
    invalidate();
}

void input_window::finish(bool accepted) {
    const std::string text = answer_.get_text();
    const auto accept = accept_;
    close(accepted ? native::dialog_result::accepted
                   : native::dialog_result::cancelled);
    if (accepted && accept)
        accept(text);
}

function_window::function_window(native::app_wnd &owner)
    : native::modal_wnd(owner, "Insert Function", 200, 160, 420, 330),
      category_(categories(), native::combo_box_style::drop_down_list, 12, 12,
                396, 26),
      names_({}, 12, 46, 396, 200), ok_("OK", 228, 290, 88, 28),
      cancel_("Cancel", 320, 290, 88, 28) {
    on_wnd_create.connect([this] {
        attach(category_);
        attach(names_);
        attach(ok_);
        attach(cancel_);
        category_.set_selected_index(0);
        show_category();
        return true;
    });
    on_wnd_paint.connect([this](native::wnd_paint_event event) {
        paint_prompt(event, summary_, native::rect(12, 254, 396, 26));
        return true;
    });
    category_.on_selection_change.connect([this](int) {
        show_category();
        return true;
    });
    names_.on_selection_change.connect([this](int) {
        show_summary();
        return true;
    });
    ok_.on_click.connect([this] {
        finish(true);
        return true;
    });
    cancel_.on_click.connect([this] {
        finish(false);
        return true;
    });
}

void function_window::attach(native::wnd &child) {
    child.set_parent(this);
    child.create();
    child.show();
}

void function_window::show_category() {
    const int index = category_.get_selected_index();
    if (index < 0)
        return;
    const std::string &category = category_.get_items()[index];
    shown_.clear();
    std::vector<std::string> labels;
    for (const expr::function_def *item : func::builtin_functions().all())
        if (item->category == category) {
            shown_.push_back(item);
            labels.emplace_back(item->name);
        }
    names_.set_items(std::move(labels));
    names_.set_selected_index(0);
    show_summary();
}

void function_window::show_summary() {
    const int index = names_.get_selected_index();
    summary_ = index >= 0 && index < static_cast<int>(shown_.size())
                   ? std::string(shown_[static_cast<std::size_t>(index)]->summary)
                   : std::string();
    invalidate();
}

void function_window::ask(std::function<void(const std::string &)> accept) {
    accept_ = std::move(accept);
    if (!get_created())
        create();
    center_to_parent();
    show();
}

void function_window::finish(bool accepted) {
    const int index = names_.get_selected_index();
    const auto accept = accept_;
    const std::string name =
        accepted && index >= 0 && index < static_cast<int>(shown_.size())
            ? std::string(shown_[static_cast<std::size_t>(index)]->name)
            : std::string();
    close(accepted ? native::dialog_result::accepted
                   : native::dialog_result::cancelled);
    if (!name.empty() && accept)
        accept(name);
}

} // namespace ui
