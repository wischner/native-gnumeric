//
// The interactive view: navigation, editing, pointing, clipboard and
// painting, against a recording painter.
//
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include <algorithm>
#include <string>
#include <vector>

#include "book/workbook.h"
#include "check.h"
#include "impexp/file_format.h"
#include "view/grid_view.h"

using book::sheet;
using val::cell_pos;
using view::commit_move;
using view::grid_view;

namespace {

cell_pos at(const char *name) {
    return *val::parse_cell_name(name);
}

// Remembers what was drawn.
class recorder final : public gfx::painter {
public:
    const gfx::palette &colors() const override { return palette_; }
    void fill_rect(const gfx::device_rect &bounds, util::color) override {
        if (selecting_ == 0)
            fills.push_back(bounds);
    }
    void frame_rect(const gfx::device_rect &bounds, util::color,
                    int) override {
        frames.push_back(bounds);
    }
    void draw_line(int, int, int, int, util::color, int) override {
        ++lines;
    }
    void draw_text(const gfx::font &, std::string_view text, int x,
                   int baseline, util::color color) override {
        texts.push_back(std::string(text));
        xs.push_back(x);
        baselines.push_back(baseline);
        inks.push_back(color);
    }
    void clipped(const gfx::device_rect &bounds,
                 const std::function<void()> &draw) override {
        clips.push_back(bounds);
        draw();
    }
    void draw_selection(
        const gfx::device_rect &bounds,
        const std::function<void(gfx::painter &)> &content) override {
        selections.push_back(bounds);
        ++selecting_;
        content(*this);
        --selecting_;
    }

    // Report whether a text was drawn.
    bool drew(const std::string &text) const {
        return std::find(texts.begin(), texts.end(), text) != texts.end();
    }

    std::vector<gfx::device_rect> fills;
    std::vector<gfx::device_rect> frames;
    std::vector<gfx::device_rect> clips;
    std::vector<gfx::device_rect> selections;
    std::vector<std::string> texts;
    std::vector<int> xs;
    std::vector<int> baselines;
    std::vector<util::color> inks;
    int lines = 0;

private:
    gfx::palette palette_;
    int selecting_ = 0;
};

// A workbook, its history and a view of it.
struct fixture {
    fixture() : stack(book), view(make(), stack, fonts) {
        view.set_viewport(800, 600);
    }

    book::workbook &make() {
        impexp::new_workbook(book);
        return book;
    }

    // Type into the cursor's cell and press Enter.
    void type(const char *text) {
        view.begin_typing(text);
        view.commit(commit_move::down);
    }

    sheet &s() { return view.active(); }

    book::workbook book;
    cmd::command_stack stack;
    gfx::fixed_font_provider fonts;
    grid_view view;
};

void test_navigation() {
    fixture f;
    grid_view &v = f.view;
    CHECK(v.cursor() == at("A1"));
    v.move_by(1, 2, false);
    CHECK(v.cursor() == at("B3"));
    CHECK(!v.has_range());
    v.move_by(1, 1, true);
    CHECK(v.cursor() == at("C4"));
    CHECK(v.current_range() == *val::parse_range_name("B3:C4"));
    CHECK(v.position_name() == "C4");
    v.move_by(-5, -5, false);
    CHECK(v.cursor() == at("A1"));
    v.add_selection(at("D4"));
    CHECK(v.selection().size() == 2);
    v.move_to(at("B2"), false);
    CHECK(v.selection().size() == 1);
    // Ctrl+arrow runs along data, then to the next block.
    for (const char *name : {"A1", "A2", "A3", "A8"})
        f.s().set_constant(at(name), val::value(1.0));
    v.move_to(at("A1"), false);
    v.jump(0, 1, false);
    CHECK(v.cursor() == at("A3"));
    v.jump(0, 1, false);
    CHECK(v.cursor() == at("A8"));
    v.jump(0, 1, false);
    CHECK(v.cursor().row == 65535);
    v.jump(1, 0, false);
    CHECK(v.cursor().col == 255);
    v.move_home(false);
    CHECK(v.cursor().col == 0);
    v.select_cols(2, 3, false);
    CHECK(v.current_range().end.row == 65535);
    CHECK(v.current_range().start.col == 2);
    v.select_rows(4, 4, false);
    CHECK(v.current_range().end.col == 255);
    v.select_all();
    CHECK(v.current_range().end == (cell_pos{255, 65535}));
    // Hidden lines are stepped over.
    v.move_to(at("B1"), false);
    f.s().cols().set_hidden(2, true);
    v.move_by(1, 0, false);
    CHECK(v.cursor() == at("D1"));
}

void test_scrolling() {
    fixture f;
    grid_view &v = f.view;
    v.move_to(at("A100"), false);
    v.reveal_cursor();
    CHECK(v.scroll_y() > 0);
    const gfx::device_rect cell = v.cell_rect(v.cursor());
    CHECK(cell.y + cell.height - v.scroll_y() <= 600);
    v.move_to(at("A1"), false);
    v.reveal_cursor();
    CHECK(v.scroll_y() == 0);
    v.set_scroll(-50, -50);
    CHECK(v.scroll_x() == 0);
    v.set_scroll(100000000, 100000000);
    CHECK(v.scroll_y() == v.content_height() - 600);
    v.move_page(-1, false);
    CHECK(v.cursor().row == 0);
    v.set_scroll(0, 0);
    v.set_zoom(2);
    CHECK(v.cell_rect(at("B1")).x == 128);
    v.set_zoom(1);
    CHECK(v.cell_at(70, 20) == at("B2"));
}

void test_merges() {
    fixture f;
    grid_view &v = f.view;
    f.s().merge(*val::parse_range_name("B2:C3"));
    v.move_to(at("B2"), false);
    CHECK(v.current_range() == *val::parse_range_name("B2:C3"));
    v.move_by(0, 1, false);
    CHECK(v.cursor() == at("B4"));
    v.move_to(at("A2"), false);
    v.move_by(1, 0, true);
    CHECK(v.current_range() == *val::parse_range_name("A2:C3"));
    const gfx::device_rect merged = v.cell_rect(at("C3"));
    CHECK(merged.width == v.range_rect(*val::parse_range_name("B2:C2")).width);
}

void test_editing() {
    fixture f;
    grid_view &v = f.view;
    f.type("12");
    CHECK(v.cursor() == at("A2"));
    CHECK(f.s().value_at(at("A1")).number() == 12);
    v.begin_typing("=A1*");
    CHECK(v.editing());
    CHECK(v.can_point());
    v.insert_text("2");
    v.commit(commit_move::right);
    CHECK(v.cursor() == at("B2"));
    CHECK(f.s().value_at(at("A2")).number() == 24);
    // F2 opens the existing entry.
    v.move_to(at("A2"), false);
    v.begin_edit();
    CHECK(v.editor().text() == "=A1*2");
    v.editor().erase_left();
    v.insert_text("3");
    v.commit();
    CHECK(f.s().value_at(at("A2")).number() == 36);
    // Escape keeps the old content.
    v.begin_typing("nope");
    v.cancel_edit();
    CHECK(f.s().value_at(at("A2")).number() == 36);
    // A malformed formula stays open and reports why.
    v.begin_typing("=1+");
    v.commit(commit_move::down);
    CHECK(v.editing());
    CHECK(!v.edit_error().empty());
    v.cancel_edit();
    // Undo goes through the same stack.
    f.stack.undo();
    CHECK(f.s().value_at(at("A2")).number() == 24);
    // Editor mechanics.
    v.begin_typing("abc");
    view::cell_editor &edit = v.editor();
    edit.move(-1, false);
    edit.insert("X");
    CHECK(edit.text() == "abXc");
    edit.move(-2, true);
    CHECK(edit.has_selection());
    edit.insert("Y");
    CHECK(edit.text() == "aYc");
    edit.select_all();
    edit.erase_right();
    CHECK(edit.text().empty());
    edit.insert("\xC4\x8D\xC5\xA1");
    edit.erase_left();
    CHECK(edit.text() == "\xC4\x8D");
    v.cancel_edit();
}

void test_pointing() {
    fixture f;
    grid_view &v = f.view;
    v.move_to(at("C3"), false);
    v.begin_typing("=SUM(");
    CHECK(v.can_point());
    v.point_at(*val::parse_range_name("A1"));
    CHECK(v.editor().text() == "=SUM(A1");
    CHECK(v.pointed().has_value());
    // Pointing again replaces what it wrote.
    v.point_at(*val::parse_range_name("A1:A5"));
    CHECK(v.editor().text() == "=SUM(A1:A5");
    v.insert_text(")");
    CHECK(!v.pointed().has_value());
    CHECK(!v.can_point());
    v.commit();
    CHECK(f.s().entry_text(at("C3")) == "=SUM(A1:A5)");
    // Plain text never points.
    v.begin_typing("hello");
    CHECK(!v.can_point());
    v.cancel_edit();
}

void test_array_and_fill_entry() {
    fixture f;
    grid_view &v = f.view;
    v.move_to(at("A1"), false);
    v.move_by(0, 2, true);
    v.begin_typing("=ROW()*2");
    v.commit_fill();
    CHECK(f.s().value_at(at("A3")).number() == 6);
    v.move_to(at("C1"), false);
    v.move_by(0, 1, true);
    v.begin_typing("={1;2}*3");
    v.commit_array();
    CHECK(f.s().value_at(at("C2")).number() == 6);
}

void test_clipboard() {
    fixture f;
    grid_view &v = f.view;
    f.type("1");
    f.type("=A1+1");
    v.move_to(at("A1"), false);
    v.move_by(0, 1, true);
    const std::string text = v.copy();
    CHECK(text == "1\n2\n");
    v.move_to(at("C5"), false);
    v.paste(text);
    CHECK(f.s().entry_text(at("C6")) == "=C5+1");
    CHECK(v.current_range() == *val::parse_range_name("C5:C6"));
    // Text from elsewhere pastes as entries.
    v.move_to(at("E1"), false);
    v.paste("7\t8\n");
    CHECK(f.s().value_at(at("F1")).number() == 8);
    // A cut moves on paste, and references follow.
    v.move_to(at("A1"), false);
    const std::string cut = v.cut();
    v.move_to(at("H1"), false);
    v.paste(cut);
    CHECK(f.s().entry_text(at("A2")) == "=H1+1");
    CHECK(f.s().find(at("A1")) == nullptr);
    CHECK(!v.clipboard().has_value());
    // Special paste: values only.
    v.move_to(at("A2"), false);
    v.copy();
    v.move_to(at("J1"), false);
    v.paste_special({true, true, true, false});
    CHECK(f.s().entry_text(at("J1")) == "2");
}

void test_fill_handle() {
    fixture f;
    grid_view &v = f.view;
    f.type("1");
    f.type("2");
    v.move_to(at("A1"), false);
    v.move_by(0, 1, true);
    const gfx::device_rect area = v.range_rect(v.current_range());
    const view::grid_hit hit =
        v.hit_at(area.x + area.width, area.y + area.height);
    CHECK(hit.kind == view::hit_kind::fill_handle);
    v.fill_to(*val::parse_range_name("A1:A5"));
    CHECK(f.s().value_at(at("A5")).number() == 5);
    f.stack.undo();
    CHECK(f.s().find(at("A5")) == nullptr);
}

void test_commands() {
    fixture f;
    grid_view &v = f.view;
    CHECK(view::run_command(v, "toggleBold"));
    CHECK(v.cursor_style().bold);
    view::run_command(v, "toggleBold");
    CHECK(!v.cursor_style().bold);
    view::run_command(v, "alignCenter");
    CHECK(v.cursor_style().horizontal == book::h_align::center);
    view::run_command(v, "alignCenter");
    CHECK(v.cursor_style().horizontal == book::h_align::general);
    view::run_command(v, "formatPercent");
    CHECK(v.cursor_style().format == "0%");
    view::run_command(v, "decimalsMore");
    CHECK(v.cursor_style().format == "0.0%");
    view::run_command(v, "decimalsMore");
    CHECK(v.cursor_style().format == "0.00%");
    view::run_command(v, "decimalsLess");
    view::run_command(v, "decimalsLess");
    CHECK(v.cursor_style().format == "0%");
    view::run_command(v, "formatGeneral");
    CHECK(!view::run_command(v, "noSuchCommand"));
    v.move_to(at("B2"), false);
    v.move_by(1, 1, true);
    view::run_command(v, "borderOutline");
    CHECK(f.s().style_at(at("B2"))->borders[0].line ==
          book::border_line::thin);
    CHECK(f.s().style_at(at("C3"))->borders[1].line ==
          book::border_line::thin);
    CHECK(f.s().style_at(at("B2"))->borders[1].line ==
          book::border_line::none);
    view::run_command(v, "mergeCells");
    CHECK(f.s().merged().size() == 1);
    view::run_command(v, "mergeCells");
    CHECK(f.s().merged().empty());
    view::run_command(v, "insertRows");
    CHECK(f.s().style_at(at("B4"))->borders[0].line ==
          book::border_line::thin);
    view::run_command(v, "undo");
    view::run_command(v, "hideCols");
    CHECK(f.s().cols().is_hidden(2));
    view::run_command(v, "unhideCols");
    CHECK(!f.s().cols().is_hidden(2));
    view::run_command(v, "toggleGrid");
    CHECK(!f.s().show_grid);
    view::run_command(v, "toggleGrid");
}

// Return everything a command could change, as text.
std::string snapshot(book::workbook &source) {
    std::string out;
    for (int i = 0; i < source.sheet_count(); ++i) {
        sheet &item = source.sheet_at(i);
        out += "[" + item.name() + "]\n";
        item.for_each_stored([&](book::cell &stored) {
            out += val::cell_name(stored.pos()) + "=" +
                   item.entry_text(stored.pos()) + "|" +
                   item.style_at(stored.pos())->format + "\n";
        });
        out += "merges " + std::to_string(item.merged().size()) + "\n";
        for (const auto &[index, entry] : item.cols().entries())
            out += "col " + std::to_string(index) + " " +
                   std::to_string(entry.points) +
                   (entry.hidden ? " hidden\n" : "\n");
        for (const auto &[index, entry] : item.rows().entries())
            out += "row " + std::to_string(index) + " " +
                   std::to_string(entry.points) +
                   (entry.hidden ? " hidden\n" : "\n");
    }
    return out;
}

// Every command, alone and with a large selection, undoes exactly.
void test_undo_fidelity() {
    static constexpr const char *names[] = {
        "clearContents", "clearFormats", "clearAll", "insertRows",
        "insertCols", "deleteRows", "deleteCols", "insertSheet",
        "deleteSheet", "sheetLeft", "sheetRight", "toggleBold",
        "toggleItalic", "toggleUline", "toggleStrike", "toggleWrap",
        "alignLeft", "alignCenter", "alignRight", "alignJustify",
        "marginIncrease", "marginDecrease", "formatGeneral", "formatNumber",
        "formatCurrency", "formatPercent", "formatDate", "formatTime",
        "formatScientific", "formatFraction", "formatText", "decimalsMore",
        "decimalsLess", "mergeCells", "borderOutline", "borderNone",
        "hideCols", "unhideCols", "hideRows", "unhideRows", "autofitCols",
        "autofitRows", "sortAscending", "sortDescending"};
    for (const int mode : {0, 1, 2}) {
        for (const char *name : names) {
            fixture f;
            grid_view &v = f.view;
            f.type("12");
            f.type("=A1*3");
            f.type("kept");
            v.move_to(at("B2"), false);
            f.type("=SUM(A1:A2)");
            f.book.sheet_at(1).set_constant(at("A1"), val::value(5.0));
            v.move_to(at("C1"), false);
            f.type("=Sheet2!A1");
            if (mode == 0) {
                v.move_to(at("A2"), false);
            } else if (mode == 1) {
                v.move_to(at("A1"), false);
                v.move_by(1, 1, true);
            } else {
                v.select_all();
            }
            f.book.recalculate();
            const std::string before = snapshot(f.book);
            int steps = 0;
            view::run_command(v, name);
            while (f.stack.can_undo() && f.stack.undo_name() != "Set cell") {
                v.undo();
                ++steps;
            }
            const std::string after = snapshot(f.book);
            if (after != before) {
                std::cerr << name << " mode " << mode << " steps " << steps
                          << " differs:\n--- before\n" << before
                          << "--- after\n" << after;
                CHECK(after == before);
                return;
            }
            CHECK(after == before);
        }
    }
}

void test_sheets() {
    fixture f;
    grid_view &v = f.view;
    CHECK(f.book.sheet_count() == 3);
    v.move_to(at("C3"), false);
    v.set_active(1);
    CHECK(v.cursor() == at("A1"));
    v.set_active(0);
    CHECK(v.cursor() == at("C3"));
    v.add_sheet();
    CHECK(v.active_index() == 1);
    CHECK(f.book.sheet_count() == 4);
    CHECK(v.active().name() == "Sheet4");
    v.undo();
    CHECK(f.book.sheet_count() == 3);
    CHECK(v.rename_sheet("Data"));
    CHECK(v.rename_sheet("Data2"));
    CHECK(v.active().name() == "Data2");
    v.set_active(0);
    CHECK(!v.rename_sheet("Data2"));
    CHECK(!v.rename_sheet(""));
    v.move_sheet(1);
    CHECK(v.active_index() == 1);
    CHECK(f.book.sheet_at(0).name() == "Data2");
    v.set_active(2);
    v.remove_sheet();
    CHECK(f.book.sheet_count() == 2);
    CHECK(v.active_index() == 1);
    v.undo();
    CHECK(f.book.sheet_count() == 3);
    // Switching sheets commits an open edit.
    v.set_active(0);
    v.begin_typing("kept");
    v.set_active(1);
    CHECK(!v.editing());
    CHECK(f.book.sheet_at(0).find(at("A1")) != nullptr);
}

void test_summary() {
    fixture f;
    grid_view &v = f.view;
    f.type("1.5");
    f.type("2");
    f.type("word");
    v.move_to(at("A1"), false);
    v.move_by(0, 2, true);
    CHECK(v.selection_summary() == "Sum=3.5");
}

void test_painting() {
    fixture f;
    grid_view &v = f.view;
    f.type("hello");
    f.type("42");
    f.type("a long string that overflows");
    book::style_patch red;
    red.back = util::color{255, 0, 0};
    red.pattern = 1;
    f.s().apply_style(*val::parse_range_name("B1"), red);
    {
        recorder target;
        v.draw(target);
        CHECK(target.drew("hello"));
        CHECK(target.drew("42"));
        CHECK(target.lines > 10);
        // The cell's fill, the sheet paper, and the cursor's handle.
        CHECK(target.fills.size() >= 3);
        CHECK(target.selections.empty());
        // Overflow clip is wider than the cell.
        bool wide = false;
        for (const gfx::device_rect &clip : target.clips)
            wide = wide || clip.width > 64;
        CHECK(wide);
    }
    // A number that does not fit shows hashes, never a lie.
    f.s().cols().set_points(1, 10, true);
    f.s().set_constant(at("B5"), val::value(123456789.0));
    book::style_patch format;
    format.format = "0.00";
    f.s().apply_style(*val::parse_range_name("B5"), format);
    {
        recorder target;
        v.draw(target);
        bool hashes = false;
        for (const std::string &text : target.texts)
            hashes = hashes || text.find('#') != std::string::npos;
        CHECK(hashes);
    }
    // A range paints the platform selection.
    v.move_to(at("A1"), false);
    v.move_by(2, 2, true);
    {
        recorder target;
        v.draw(target);
        CHECK(target.selections.size() == 1);
    }
    // The open editor shows its text.
    v.begin_typing("typing");
    {
        recorder target;
        v.draw(target);
        CHECK(target.drew("typing"));
    }
    v.cancel_edit();
    // Formulas mode shows entries.
    view::run_command(v, "toggleFormulas");
    f.s().set_constant(at("D1"), val::value(1.0));
    {
        recorder target;
        v.draw(target);
        CHECK(target.drew("1"));
    }
}

} // namespace

int main() {
    test_navigation();
    test_scrolling();
    test_merges();
    test_editing();
    test_pointing();
    test_array_and_fill_entry();
    test_clipboard();
    test_fill_handle();
    test_commands();
    test_undo_fidelity();
    test_sheets();
    test_summary();
    test_painting();
    return testing::summary("view");
}
