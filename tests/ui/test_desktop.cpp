//
// Drives the real application window through Native's public input
// entry points, then verifies the saved file, the view and the painted
// pixels. Nothing here names a toolkit, so one source runs on every
// Native backend; a backend supplies only its display.
//
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <stop_token>
#include <string>
#include <thread>

#include <unistd.h>

#include "check.h"
#include "impexp/file_format.h"
#include "main_window.h"
#include "tool_icons.h"

namespace fs = std::filesystem;
using native::key_code;
using native::key_modifiers;
using val::cell_pos;

// Runs the scenario against one window.
class desktop_test {
public:
    explicit desktop_test(ui::main_window &window)
        : window_(window), canvas_(window.canvas_),
          path_(fs::temp_directory_path() /
                ("isaac-test-" + std::to_string(getpid()) + ".gnumeric")) {}

    ~desktop_test() {
        std::error_code ignored;
        fs::remove(path_, ignored);
    }

    // Run everything that needs no fresh paint.
    void run_edits() {
        CHECK(canvas_.get_created());
        test_resources();
        test_chrome();
        test_typing();
        test_navigation();
        test_formula_entry();
        test_toolbar_and_menu();
        test_format_bar();
        test_tabs();
        test_mouse();
        test_headers();
        test_clipboard();
        test_find();
        test_save_and_load();
        test_every_command();
        select(0);
        canvas_.grid().move_to({0, 0}, false);
        canvas_.refresh();
    }

    // Verify what the last paint produced.
    void check_paint() {
        const native::img *buffer = canvas_.buffer_.get();
        CHECK(buffer != nullptr);
        if (!buffer)
            return;
        if (const char *shot = std::getenv("ISAAC_TEST_SHOT"))
            buffer->save(shot);
        const native::rgba *pixels = buffer->pixels();
        const int count = buffer->w() * buffer->h();
        int dark = 0;
        for (int i = 0; i < count; ++i)
            dark += pixels[i].r < 128 ? 1 : 0;
        CHECK(dark > 40);
        CHECK(dark < count / 2);
    }

    native::canvas &canvas() { return canvas_; }

private:
    view::grid_view &grid() { return canvas_.grid(); }
    book::sheet &sheet() { return grid().active(); }

    void select(int index) { grid().set_active(index); }

    // Press a physical key with modifiers.
    void press(key_code key, key_modifiers modifiers = key_modifiers::none) {
        canvas_.on_native_key({key, native::key_action::press, false,
                               modifiers});
        canvas_.on_native_key({key, native::key_action::release, false,
                               modifiers});
    }

    // Deliver composed text as a backend does: Native suppresses text
    // belonging to the last handled shortcut, so a fresh unhandled
    // physical key precedes it.
    void type(const std::string &text) {
        native::key_event event;
        event.key = key_code::a;
        event.action = native::key_action::press;
        canvas_.on_native_key(event);
        canvas_.on_native_text_input(text);
        event.action = native::key_action::release;
        canvas_.on_native_key(event);
    }

    // Press the mouse on a cell.
    void click_cell(cell_pos pos, int clicks = 1) {
        const gfx::device_rect cell = grid().cell_rect(pos);
        const native::rect client = canvas_.get_client_bounds();
        const native::point at(
            static_cast<native::coord>(client.p.x + cell.x + 4 -
                                       grid().scroll_x()),
            static_cast<native::coord>(client.p.y + cell.y + 4 -
                                       grid().scroll_y()));
        native::mouse_event event(native::mouse_button::left,
                                  native::mouse_action::press, at);
        event.clicks = clicks;
        canvas_.wnd::on_native_mouse_click(event);
        canvas_.wnd::on_native_mouse_click(native::mouse_event(
            native::mouse_button::left, native::mouse_action::release, at));
    }

    void test_resources() {
        for (const char *name :
             {"fileNew", "undo", "toggleBold", "alignLeft", "autoSum",
              "sortAscending", "mergeCells", "formatPercent", "borderOutline"})
            CHECK(ui::tool_icon(name) != nullptr);
        CHECK(ui::tool_icon("noSuchTool") == nullptr);
    }

    // The window is Writer's shape: three strips, status, canvas strips.
    void test_chrome() {
        CHECK(window_.toolbar_.get_items().size() > 10);
        CHECK(window_.format_bar_.get_items().size() > 10);
        CHECK(window_.status_.get_extent() ==
              native::theme::create(window_.get_gpx())->get_status_bar_height());
        CHECK(window_.status_.get_parts().size() == 4);
        CHECK(window_.status_.get_parts()[0].width == 130);
        CHECK(window_.status_.get_parts()[0].text == "Sheet 1 of 3");
        CHECK(window_.status_.get_parts()[1].width == 150);
        CHECK(window_.status_.get_parts()[2].width == 70);
        CHECK(window_.status_.get_parts()[2].text == "100%");
        CHECK(window_.tabs_.get_items().size() == 3);
        CHECK(window_.get_title() == "Untitled - Isaac");
        CHECK(canvas_.get_client_bounds().d.w > 0);
        CHECK(window_.formula_.get_created());
        // The headers sit inside the canvas, around the client.
        CHECK(canvas_.get_client_bounds().p.y >= window_.col_header_.get_extent());
        CHECK(canvas_.get_client_bounds().p.x >= window_.row_header_.get_extent());
    }

    void test_typing() {
        type("12");
        CHECK(grid().editing());
        CHECK(grid().editor().text() == "12");
        press(key_code::enter);
        CHECK(!grid().editing());
        CHECK(sheet().value_at({0, 0}).number() == 12);
        CHECK(grid().cursor() == (cell_pos{0, 1}));
        CHECK(window_.get_title() == "*Untitled - Isaac");
        type("=A1*2");
        press(key_code::tab);
        CHECK(sheet().value_at({0, 1}).number() == 24);
        CHECK(grid().cursor() == (cell_pos{1, 1}));
        type("gone");
        press(key_code::escape);
        CHECK(sheet().find({1, 1}) == nullptr);
        press(key_code::left);
        press(key_code::f2);
        CHECK(grid().editor().text() == "=A1*2");
        press(key_code::backspace);
        type("3");
        press(key_code::enter);
        CHECK(sheet().value_at({0, 1}).number() == 36);
        grid().move_to({0, 1}, false);
        press(key_code::delete_key);
        CHECK(sheet().find({0, 1}) == nullptr);
        window_.run("undo");
        CHECK(sheet().value_at({0, 1}).number() == 36);
    }

    void test_navigation() {
        grid().move_to({0, 0}, false);
        press(key_code::right);
        press(key_code::down);
        CHECK(grid().cursor() == (cell_pos{1, 1}));
        press(key_code::up, key_modifiers::shift);
        CHECK(grid().current_range() == *val::parse_range_name("B1:B2"));
        press(key_code::home, key_modifiers::ctrl);
        CHECK(grid().cursor() == (cell_pos{0, 0}));
        press(key_code::page_down);
        CHECK(grid().cursor().row > 5);
        CHECK(canvas_.get_scroll_position().y > 0);
        press(key_code::home, key_modifiers::ctrl);
        CHECK(canvas_.get_scroll_position().y == 0);
    }

    void test_formula_entry() {
        grid().move_to({3, 0}, false);
        type("=SUM(");
        CHECK(grid().can_point());
        press(key_code::left);
        CHECK(grid().editor().text() == "=SUM(C1");
        press(key_code::left, key_modifiers::shift);
        CHECK(grid().editor().text() == "=SUM(B1:C1");
        type(")");
        press(key_code::enter);
        CHECK(sheet().entry_text({3, 0}) == "=SUM(B1:C1)");
        // Clicking a cell writes its reference.
        grid().move_to({3, 2}, false);
        type("=");
        click_cell({0, 0});
        CHECK(grid().editor().text() == "=A1");
        type("+1");
        press(key_code::enter);
        CHECK(sheet().value_at({3, 2}).number() == 13);
        // A malformed formula keeps the editor open and says why.
        type("=1+");
        press(key_code::enter);
        CHECK(grid().editing());
        CHECK(!window_.status_.get_parts()[3].text.empty());
        press(key_code::escape);
    }

    void test_toolbar_and_menu() {
        grid().move_to({0, 0}, false);
        window_.toolbar_.on_native_command("undo");
        window_.toolbar_.on_native_command("redo");
        window_.format_bar_.on_native_command("toggleBold");
        CHECK(grid().cursor_style().bold);
        CHECK(window_.format_bar_.get_checked("toggleBold"));
        window_.format_bar_.on_native_command("alignCenter");
        CHECK(grid().cursor_style().horizontal == book::h_align::center);
        CHECK(window_.format_bar_.get_checked("alignCenter"));
        window_.format_bar_.on_native_command("formatPercent");
        CHECK(grid().cursor_style().format == "0%");
        window_.toolbar_.on_native_command("autoSum");
        CHECK(grid().editing());
        press(key_code::escape);
        window_.run("undo");
        window_.run("undo");
        window_.run("undo");
        grid().move_to({0, 0}, false);
    }

    // Every menu row runs, then undoes, on a loaded workbook.
    void test_every_command() {
        for (const std::string &command : window_.menu_commands_) {
            if (command == "quit" || command == "fileOpen" ||
                command == "fileNew" || command == "fileSave" ||
                command == "fileSaveAs" || command == "find" ||
                command == "goTo" || command == "about" ||
                command == "renameSheet" || command == "columnWidth" ||
                command == "rowHeight" || command == "customFormat" ||
                command == "defineName" || command == "insertFunction")
                continue;
            window_.run(command);
            CHECK(!grid().editing() || command == "autoSum");
            if (grid().editing())
                press(key_code::escape);
        }
        for (int i = 0; i < 100 && grid().commands().can_undo(); ++i)
            window_.run("undo");
        CHECK(!grid().commands().can_undo());
        select(0);
        CHECK(sheet().value_at({1, 1}).string() == "kept");
        CHECK(sheet().value_at({0, 1}).number() == 36);
    }

    void test_format_bar() {
        select(0);
        grid().move_to({5, 5}, false);
        const auto pick = [](native::combo_box &box, int index) {
            box.on_native_selection(index);
        };
        pick(window_.size_box_, 7);
        CHECK(grid().cursor_style().font_size == 18);
        CHECK(sheet().rows().points(5) > 18);
        pick(window_.color_box_, 2);
        CHECK(grid().cursor_style().fore == (util::color{192, 57, 43}));
        pick(window_.fill_box_, 6);
        CHECK(grid().cursor_style().pattern == 1);
        pick(window_.number_box_, 2);
        CHECK(grid().cursor_style().format == "$#,##0.00");
        CHECK(window_.number_box_.get_selected_index() == 2);
        pick(window_.zoom_box_, 4);
        CHECK(grid().zoom() == 1.5);
        CHECK(window_.status_.get_parts()[2].text == "150%");
        window_.run("zoomReset");
        if (window_.font_box_.get_items().size() > 1) {
            pick(window_.font_box_, 1);
            CHECK(grid().cursor_style().font_name ==
                  window_.font_box_.get_items()[1]);
        }
        window_.run("undo");
        window_.run("undo");
        window_.run("undo");
        window_.run("undo");
    }

    void test_tabs() {
        native::tab_strip &tabs = window_.tabs_;
        tabs.on_selection_change.emit(1);
        CHECK(grid().active_index() == 1);
        CHECK(window_.status_.get_parts()[0].text == "Sheet 2 of 3");
        window_.run("insertSheet");
        CHECK(tabs.get_items().size() == 4);
        CHECK(tabs.get_selected_index() == 2);
        window_.run("deleteSheet");
        CHECK(tabs.get_items().size() == 3);
        window_.run("sheetPrevious");
        window_.run("undo");
        window_.run("undo");
        select(0);
        window_.run("sheetNext");
        CHECK(grid().active_index() == 1);
        select(0);
    }

    void test_mouse() {
        click_cell({2, 3});
        CHECK(grid().cursor() == (cell_pos{2, 3}));
        click_cell({0, 0}, 2);
        CHECK(grid().editing());
        press(key_code::escape);
        // Dragging extends the selection.
        const native::rect client = canvas_.get_client_bounds();
        const native::point start(static_cast<native::coord>(client.p.x + 4),
                                  static_cast<native::coord>(client.p.y + 4));
        const native::point end(static_cast<native::coord>(client.p.x + 140),
                                static_cast<native::coord>(client.p.y + 60));
        canvas_.wnd::on_native_mouse_click(native::mouse_event(
            native::mouse_button::left, native::mouse_action::press, start));
        canvas_.on_mouse_move.emit(end);
        canvas_.wnd::on_native_mouse_click(native::mouse_event(
            native::mouse_button::left, native::mouse_action::release, end));
        CHECK(grid().has_range());
        CHECK(window_.status_.get_parts()[1].text.rfind("Sum=", 0) == 0);
        grid().move_to({0, 0}, false);
        canvas_.refresh();
    }

    void test_headers() {
        const native::rect client = canvas_.get_client_bounds();
        const native::rect bar = window_.col_header_.get_bounds();
        const native::point on_col(static_cast<native::coord>(client.p.x + 100),
                                   static_cast<native::coord>(bar.p.y + 4));
        canvas_.wnd::on_native_mouse_click(native::mouse_event(
            native::mouse_button::left, native::mouse_action::press, on_col));
        canvas_.wnd::on_native_mouse_click(native::mouse_event(
            native::mouse_button::left, native::mouse_action::release, on_col));
        CHECK(grid().current_range().end.row == sheet().row_count() - 1);
        CHECK(grid().current_range().start.col == 1);
        const native::rect side = window_.row_header_.get_bounds();
        const native::point on_row(static_cast<native::coord>(side.p.x + 4),
                                   static_cast<native::coord>(client.p.y + 42));
        canvas_.wnd::on_native_mouse_click(native::mouse_event(
            native::mouse_button::left, native::mouse_action::press, on_row));
        canvas_.wnd::on_native_mouse_click(native::mouse_event(
            native::mouse_button::left, native::mouse_action::release, on_row));
        CHECK(grid().current_range().end.col == sheet().col_count() - 1);
        // Dragging the column edge resizes it.
        const int edge = static_cast<int>(sheet().cols().offset(1));
        const native::point at(static_cast<native::coord>(client.p.x + edge),
                               static_cast<native::coord>(bar.p.y + 4));
        const native::point to(static_cast<native::coord>(client.p.x + edge + 40),
                               static_cast<native::coord>(bar.p.y + 4));
        const double before = sheet().cols().points(0);
        canvas_.wnd::on_native_mouse_click(native::mouse_event(
            native::mouse_button::left, native::mouse_action::press, at));
        canvas_.wnd::on_native_mouse_click(native::mouse_event(
            native::mouse_button::left, native::mouse_action::release, to));
        CHECK(sheet().cols().points(0) > before);
        window_.run("undo");
        CHECK(sheet().cols().points(0) == before);
        grid().move_to({0, 0}, false);
    }

    void test_clipboard() {
        grid().move_to({0, 0}, false);
        grid().move_by(0, 1, true);
        window_.run("copy");
        grid().move_to({6, 0}, false);
        window_.run("paste");
        CHECK(sheet().value_at({6, 0}).number() == 12);
        CHECK(sheet().entry_text({6, 1}) == "=G1*3");
        window_.run("undo");
        grid().clear_clipboard();
        grid().move_to({0, 0}, false);
    }

    void test_find() {
        window_.run("find");
        window_.find_.needle_.set_text("=A1");
        window_.find_.find_button_.on_click.emit();
        CHECK(grid().cursor() == (cell_pos{0, 1}));
        window_.find_.replacement_.set_text("=A1+");
        window_.find_.destroy();
    }

    void test_save_and_load() {
        grid().move_to({1, 1}, false);
        type("kept");
        press(key_code::enter);
        CHECK(window_.save_to(path_));
        CHECK(!grid().commands().modified());
        CHECK(window_.get_title().rfind(path_.filename().string(), 0) == 0);
        book::workbook saved;
        CHECK(impexp::load_file(path_, saved).ok);
        CHECK(saved.sheet_at(0).value_at({1, 1}).string() == "kept");
        CHECK(saved.sheet_at(0).value_at({0, 1}).number() == 36);
        window_.load(path_);
        CHECK(sheet().value_at({1, 1}).string() == "kept");
        CHECK(!grid().commands().can_undo());
        window_.load(fs::path(FIXTURES) / "formula-tests.gnumeric");
        CHECK(grid().workbook().sheet_count() == 3);
        CHECK(window_.tabs_.get_items()[0] == "Sheet1");
        window_.load(path_);
    }

    ui::main_window &window_;
    ui::grid_canvas &canvas_;
    fs::path path_;
};

namespace {

// Paints wait on the event loop, so a timer posts checks back to it.
class paint_waiter {
public:
    paint_waiter(desktop_test &test, ui::main_window &window, const int &paints)
        : test_(test), window_(window), paints_(paints) {}

    ~paint_waiter() { stop(); }

    // Start waiting for a paint newer than now.
    void start() {
        seen_ = paints_;
        deadline_ = std::chrono::steady_clock::now() + paint_timeout;
        timer_ = std::jthread([this](std::stop_token stop) {
            while (!stop.stop_requested()) {
                std::this_thread::sleep_for(paint_poll);
                native::app::post([this] { poll(); });
            }
        });
    }

    // Stop the timer.
    void stop() {
        if (timer_.joinable()) {
            timer_.request_stop();
            timer_.join();
        }
    }

private:
    static constexpr auto paint_poll = std::chrono::milliseconds(5);
    static constexpr auto paint_timeout = std::chrono::seconds(10);

    void poll() {
        if (finished_ || (paints_ == seen_ &&
                          std::chrono::steady_clock::now() < deadline_))
            return;
        finished_ = true;
        CHECK(paints_ != seen_);
        if (paints_ != seen_)
            test_.check_paint();
        window_.destroy();
    }

    desktop_test &test_;
    ui::main_window &window_;
    const int &paints_;
    int seen_ = 0;
    bool finished_ = false;
    std::chrono::steady_clock::time_point deadline_;
    std::jthread timer_;
};

} // namespace

int program(int, char **) {
    app::options settings;
    ui::main_window window(settings);
    desktop_test test(window);
    int paints = 0;
    paint_waiter waiter(test, window, paints);
    window.canvas_probe().on_wnd_paint.connect(
        [&paints](native::wnd_paint_event) {
            ++paints;
            return false;
        });
    window.on_wnd_create.connect([&] {
        native::app::post([&] {
            test.run_edits();
            test.canvas().invalidate();
            waiter.start();
        });
        return false;
    });
    const int status = native::app::run(window);
    waiter.stop();
    return status ? status : testing::summary("desktop");
}
