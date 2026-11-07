//
// Input and painting for the grid. Composed characters arrive through
// Native's text input path, so every layout and dead key works; physical
// keys navigate.
//
// Derived from Gnumeric src/gnm-pane.c and src/item-cursor.c,
// Copyright (C) 1998-2008 Miguel de Icaza, Jody Goldberg, Morten Welinder.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include "grid_canvas.h"

#include <algorithm>

#include "impexp/file_format.h"

namespace ui {
namespace {

using native::key_code;
using native::key_modifiers;
using view::commit_move;

constexpr int drag_scroll_step = 24;

// Report whether a modifier bit is set.
bool held(key_modifiers state, key_modifiers bit) {
    return (state & bit) == bit;
}

} // namespace

grid_canvas::grid_canvas(font_catalog &fonts)
    : native::canvas(0, 0, 640, 480), fonts_(fonts) {
    set_cursor(native::mouse_cursor::crosshair);
    set_horizontal_scrollbar_policy(native::scrollbar_policy::always);
    set_vertical_scrollbar_policy(native::scrollbar_policy::always);
    on_wnd_paint.connect([this](native::wnd_paint_event event) {
        paint(event);
        return true;
    });
    on_mouse_click.connect([this](native::mouse_event event) {
        click(event);
        return true;
    });
    on_mouse_move.connect([this](native::point position) {
        drag(position);
        return true;
    });
    on_key.connect([this](native::key_event event) { return key(event); });
    on_text_input.connect(
        [this](const std::string &utf8) { return text(utf8); });
    on_scroll.connect([this](native::canvas_scroll_position) {
        scrolled();
        return true;
    });
    on_wnd_resize.connect([this](native::size) {
        sync_viewport();
        return true;
    });
    auto fresh = std::make_unique<book::workbook>();
    impexp::new_workbook(*fresh);
    open(std::move(fresh));
}

void grid_canvas::open(std::unique_ptr<book::workbook> source) {
    view_.reset();
    stack_.reset();
    book_ = std::move(source);
    stack_ = std::make_unique<cmd::command_stack>(*book_);
    view_ = std::make_unique<view::grid_view>(*book_, *stack_, fonts_);
    sync_viewport();
    view_->reveal_cursor();
    refresh();
}

void grid_canvas::set_change_handler(std::function<void()> handler) {
    on_change_ = std::move(handler);
}

void grid_canvas::set_command_handler(
    std::function<void(std::string_view)> handler) {
    on_command_ = std::move(handler);
}

void grid_canvas::sync_viewport() {
    if (!view_)
        return;
    const native::rect client = get_client_bounds();
    view_->set_viewport(client.d.w, client.d.h);
    const auto content = get_content_bounds();
    const auto width = static_cast<std::uint32_t>(view_->content_width());
    const auto height = static_cast<std::uint32_t>(view_->content_height());
    if (content.width != width || content.height != height)
        set_content_bounds({0, 0, width, height});
    buffer_dirty_ = true;
    invalidate(client);
}

void grid_canvas::refresh() {
    sync_viewport();
    set_scroll_position({view_->scroll_x(), view_->scroll_y()});
    if (on_change_)
        on_change_();
}

void grid_canvas::scrolled() {
    const native::canvas_scroll_position scroll = get_scroll_position();
    view_->set_scroll(scroll.x, scroll.y);
    buffer_dirty_ = true;
    invalidate(get_client_bounds());
    if (on_change_)
        on_change_();
}

void grid_canvas::paint(native::wnd_paint_event event) {
    const native::rect client = get_client_bounds();
    if (client.d.w == 0 || client.d.h == 0)
        return;
    if (!buffer_ || buffer_->w() != static_cast<int>(client.d.w) ||
        buffer_->h() != static_cast<int>(client.d.h)) {
        buffer_ = std::make_unique<native::img>(client.d.w, client.d.h);
        buffer_dirty_ = true;
    }
    if (buffer_dirty_) {
        native::gpx &target = buffer_->get_gpx();
        target.set_clip(native::rect(0, 0, client.d.w, client.d.h));
        native_painter painter(target);
        view_->draw(painter);
        buffer_dirty_ = false;
    }
    event.g.draw_img(*buffer_, client.p);
}

native::point grid_canvas::to_content(native::point position) const {
    const native::rect client = get_client_bounds();
    const native::canvas_scroll_position scroll = get_scroll_position();
    return native::point(
        static_cast<native::coord>(position.x - client.p.x + scroll.x),
        static_cast<native::coord>(position.y - client.p.y + scroll.y));
}

void grid_canvas::click(native::mouse_event event) {
    if (event.button != native::mouse_button::left)
        return;
    view::grid_view &grid = *view_;
    if (event.action == native::mouse_action::release) {
        if (gesture_ == gesture::fill) {
            const val::cell_pos at = grid.cell_at(to_content(event.position).x,
                                                  to_content(event.position).y);
            val::cell_range target = grid.current_range();
            const val::cell_range source = target;
            // A fill grows along the axis it moved further on.
            const int down = at.row > source.end.row
                                 ? at.row - source.end.row
                                 : (at.row < source.start.row
                                        ? source.start.row - at.row
                                        : 0);
            const int across = at.col > source.end.col
                                   ? at.col - source.end.col
                                   : (at.col < source.start.col
                                          ? source.start.col - at.col
                                          : 0);
            if (down >= across) {
                target.start.row = std::min(target.start.row, at.row);
                target.end.row = std::max(target.end.row, at.row);
            } else {
                target.start.col = std::min(target.start.col, at.col);
                target.end.col = std::max(target.end.col, at.col);
            }
            grid.fill_to(target);
            refresh();
        }
        gesture_ = gesture::none;
        return;
    }
    const native::point content = to_content(event.position);
    const view::grid_hit hit = grid.hit_at(content.x, content.y);
    const bool shift = held(event.modifiers, key_modifiers::shift);
    const bool ctrl = held(event.modifiers, key_modifiers::ctrl);
    if (grid.editing() && grid.can_point()) {
        gesture_ = gesture::point;
        gesture_anchor_ = hit.pos;
        grid.point_at(val::cell_range::single(hit.pos));
        refresh();
        return;
    }
    if (grid.editing()) {
        grid.commit();
        if (grid.editing()) {
            refresh();
            return;
        }
    }
    if (hit.kind == view::hit_kind::fill_handle && !shift && !ctrl) {
        gesture_ = gesture::fill;
        refresh();
        return;
    }
    gesture_ = gesture::select;
    if (shift)
        grid.move_to(hit.pos, true);
    else if (ctrl)
        grid.add_selection(hit.pos);
    else
        grid.move_to(hit.pos, false);
    if (event.clicks == 2 && !shift && !ctrl) {
        gesture_ = gesture::none;
        grid.begin_edit();
    }
    focus();
    refresh();
}

void grid_canvas::drag(native::point position) {
    if (gesture_ == gesture::none)
        return;
    const native::rect client = get_client_bounds();
    native::canvas_scroll_position scroll = get_scroll_position();
    if (position.y < client.p.y)
        scroll.y -= drag_scroll_step;
    else if (position.y > client.p.y + client.d.h)
        scroll.y += drag_scroll_step;
    if (position.x < client.p.x)
        scroll.x -= drag_scroll_step;
    else if (position.x > client.p.x + client.d.w)
        scroll.x += drag_scroll_step;
    set_scroll_position(scroll);
    view::grid_view &grid = *view_;
    grid.set_scroll(scroll.x, scroll.y);
    const native::point content = to_content(position);
    const val::cell_pos at = grid.cell_at(content.x, content.y);
    if (gesture_ == gesture::select) {
        const val::cell_pos before = grid.cursor();
        if (at == before)
            return;
        grid.move_to(at, true);
    } else if (gesture_ == gesture::point) {
        grid.point_at(val::cell_range::between(gesture_anchor_, at));
    } else {
        grid.set_fill_preview(val::cell_range::between(
            grid.current_range().start, at));
    }
    refresh();
}

void grid_canvas::point_key(int cols, int rows, bool extend) {
    view::grid_view &grid = *view_;
    if (!key_point_ || !grid.pointed())
        key_point_ = grid.cursor();
    const val::cell_pos start = extend && grid.pointed()
                                    ? grid.pointed()->start
                                    : *key_point_;
    key_point_ = val::cell_pos{
        std::clamp(key_point_->col + cols, 0, grid.active().col_count() - 1),
        std::clamp(key_point_->row + rows, 0, grid.active().row_count() - 1)};
    grid.point_at(extend ? val::cell_range::between(start, *key_point_)
                         : val::cell_range::single(*key_point_));
}

bool grid_canvas::editing_key(native::key_event event, bool ctrl, bool shift) {
    view::grid_view &grid = *view_;
    view::cell_editor &edit = grid.editor();
    int cols = 0;
    int rows = 0;
    switch (event.key) {
    case key_code::enter:
    case key_code::keypad_enter:
        if (ctrl && shift)
            grid.commit_array();
        else if (ctrl)
            grid.commit_fill();
        else
            grid.commit(shift ? commit_move::up : commit_move::down);
        break;
    case key_code::tab:
        grid.commit(shift ? commit_move::left : commit_move::right);
        break;
    case key_code::escape:
        grid.cancel_edit();
        key_point_.reset();
        break;
    case key_code::backspace:
        edit.erase_left();
        break;
    case key_code::delete_key:
        edit.erase_right();
        break;
    case key_code::home:
        edit.set_caret(0, shift);
        break;
    case key_code::end:
        edit.set_caret(edit.characters().size(), shift);
        break;
    case key_code::left:
        cols = -1;
        break;
    case key_code::right:
        cols = 1;
        break;
    case key_code::up:
        rows = -1;
        break;
    case key_code::down:
        rows = 1;
        break;
    default:
        return false;
    }
    if (cols != 0 || rows != 0) {
        if (grid.can_point())
            point_key(cols, rows, shift);
        else if (rows == 0)
            edit.move(cols, shift);
        else
            grid.commit(rows > 0 ? commit_move::down : commit_move::up);
    }
    refresh();
    return true;
}

bool grid_canvas::grid_key(native::key_event event, bool ctrl, bool shift) {
    view::grid_view &grid = *view_;
    const auto step = [&](int cols, int rows) {
        if (ctrl)
            grid.jump(cols, rows, shift);
        else
            grid.move_by(cols, rows, shift);
    };
    switch (event.key) {
    case key_code::left:
        step(-1, 0);
        break;
    case key_code::right:
        step(1, 0);
        break;
    case key_code::up:
        step(0, -1);
        break;
    case key_code::down:
        step(0, 1);
        break;
    case key_code::page_up:
        grid.move_page(-1, shift);
        break;
    case key_code::page_down:
        grid.move_page(1, shift);
        break;
    case key_code::home:
        if (ctrl) {
            grid.move_to({0, 0}, shift);
            grid.reveal_cursor();
        } else {
            grid.move_home(shift);
        }
        break;
    case key_code::end:
        if (ctrl) {
            if (const auto used = grid.active().used_range()) {
                grid.move_to(used->end, shift);
                grid.reveal_cursor();
            }
        }
        break;
    case key_code::enter:
    case key_code::keypad_enter:
        grid.move_by(0, shift ? -1 : 1, false);
        break;
    case key_code::tab:
        grid.move_by(shift ? -1 : 1, 0, false);
        break;
    case key_code::delete_key:
        grid.clear_selection(cmd::clear_kind::contents);
        break;
    case key_code::backspace:
        grid.begin_typing("");
        break;
    case key_code::escape:
        grid.clear_clipboard();
        break;
    case key_code::f2:
        grid.begin_edit();
        break;
    default:
        return false;
    }
    refresh();
    return true;
}

bool grid_canvas::key(native::key_event event) {
    if (event.action != native::key_action::press)
        return false;
    const bool ctrl = held(event.modifiers, key_modifiers::ctrl);
    const bool shift = held(event.modifiers, key_modifiers::shift);
    if (event.key == key_code::page_up && ctrl) {
        if (on_command_)
            on_command_("sheetPrevious");
        return true;
    }
    if (event.key == key_code::page_down && ctrl) {
        if (on_command_)
            on_command_("sheetNext");
        return true;
    }
    return view_->editing() ? editing_key(event, ctrl, shift)
                            : grid_key(event, ctrl, shift);
}

bool grid_canvas::text(const std::string &utf8) {
    std::string typed;
    for (const char c : utf8)
        if (static_cast<unsigned char>(c) >= 0x20 && c != 0x7f)
            typed.push_back(c);
    if (typed.empty())
        return false;
    key_point_.reset();
    view_->insert_text(typed);
    refresh();
    return true;
}

} // namespace ui
