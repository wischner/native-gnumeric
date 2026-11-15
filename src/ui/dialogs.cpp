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
#include "form_layout.h"

#include <algorithm>
#include <set>
#include <sstream>

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

} // namespace

input_window::input_window(native::app_wnd &owner)
    : native::modal_wnd(owner, "Isaac", 200, 200, 1, 1),
      answer_("", native::text_edit_mode::single_line),
      ok_("OK"), cancel_("Cancel") {
    on_wnd_create.connect([this] {
        attach_form_control(*this, answer_, 1);
        attach_form_control(*this, ok_, 2);
        attach_form_control(*this, cancel_, 3);
        layout();
        return true;
    });
    on_wnd_paint.connect([this](native::wnd_paint_event event) {
        const auto m = measure_form(*this);
        const auto area = form_area(*this, m);
        paint_form_label(event, prompt_,
                         native::rect(area.x1(), area.y1(), area.w(), m.line));
        return true;
    });
    on_wnd_resize.connect([this](native::size) {
        if (answer_.get_created()) layout();
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

void input_window::layout() {
    const auto m = measure_form(*this);
    auto &graphics = get_gpx();
    auto saved = graphics.save_state();
    graphics.set_font(native::font_t::stock(native::font_role::control));
    const int width = std::max(3 * m.button_width + 2 * m.gap,
                              graphics.measure_text(prompt_).width);
    fit_form(*this, width + 2 * m.margin,
             2 * m.margin + m.line + m.field + m.button + 3 * m.gap);
    native::grid_layout_manager grid;
    grid << native::column(native::star())
         << native::column(native::pixels(m.button_width))
         << native::column(native::pixels(m.gap))
         << native::column(native::pixels(m.button_width));
    for (int height : {m.line, m.gap, m.field, 2 * m.gap})
        grid << native::row(native::pixels(height));
    grid << native::row(native::star()) << native::row(native::pixels(m.button));
    grid << native::cell(answer_, 2, 0, 1, 4)
         << native::cell(ok_, 5, 1) << native::cell(cancel_, 5, 3);
    grid.relayout(this, form_area(*this, m));
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
    layout();
    center_form(*this);
    show();
    answer_.focus();
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
    : native::modal_wnd(owner, "Insert Function", 200, 160, 1, 1),
      category_(categories(), native::combo_box_style::drop_down_list),
      names_(), ok_("OK"), cancel_("Cancel"),
      summary_("", native::text_edit_mode::multi_line) {
    summary_.set_read_only(true);
    on_wnd_create.connect([this] {
        attach_form_control(*this, category_, 1);
        attach_form_control(*this, names_, 2);
        attach_form_control(*this, summary_, 3);
        attach_form_control(*this, ok_, 4);
        attach_form_control(*this, cancel_, 5);
        category_.set_selected_index(0);
        show_category();
        layout();
        return true;
    });
    on_wnd_resize.connect([this](native::size) {
        if (cancel_.get_created()) layout();
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

void function_window::layout() {
    const auto m = measure_form(*this);
    const int list_height = 8 * native::theme::create(get_gpx())->get_list_item_height();
    const int summary_height = 3 * m.line + 2 * m.gap;
    fit_form(*this, 4 * m.button_width + 2 * m.margin,
             2 * m.margin + m.field + list_height + summary_height +
             m.button + 3 * m.gap);
    native::grid_layout_manager grid;
    grid << native::column(native::star())
         << native::column(native::pixels(m.button_width))
         << native::column(native::pixels(m.gap))
         << native::column(native::pixels(m.button_width));
    grid << native::row(native::pixels(m.field))
         << native::row(native::pixels(m.gap)) << native::row(native::star())
         << native::row(native::pixels(m.gap))
         << native::row(native::pixels(summary_height))
         << native::row(native::pixels(m.gap))
         << native::row(native::pixels(m.button));
    grid << native::cell(category_, 0, 0, 1, 4)
         << native::cell(names_, 2, 0, 1, 4)
         << native::cell(summary_, 4, 0, 1, 4)
         << native::cell(ok_, 6, 1) << native::cell(cancel_, 6, 3);
    grid.relayout(this, form_area(*this, m));
    show_summary();
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
    const std::string text = index >= 0 && index < static_cast<int>(shown_.size())
        ? std::string(shown_[static_cast<std::size_t>(index)]->summary) : "";
    // Native's multiline editors do not all wrap automatically. Insert line
    // breaks measured with the same control font, retaining keyboard scrolling.
    const auto &font = native::font_t::stock(native::font_role::control);
    const int width = std::max(1, int(summary_.get_bounds().w()) -
                                  2 * measure_form(*this).gap);
    std::istringstream words(text);
    std::string word, line, wrapped;
    while (words >> word) {
        const std::string candidate = line.empty() ? word : line + " " + word;
        if (!line.empty() && font.measure_text(candidate).width > width) {
            wrapped += line + "\n";
            line = word;
        } else {
            line = candidate;
        }
    }
    summary_.set_text(wrapped + line);
    invalidate();
}

void function_window::ask(std::function<void(const std::string &)> accept) {
    accept_ = std::move(accept);
    if (!get_created())
        create();
    center_form(*this);
    show();
    category_.focus();
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
