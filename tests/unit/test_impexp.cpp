//
// File formats: round trips, Gnumeric's own samples, undo.
//
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

#include "book/region.h"
#include "book/workbook.h"
#include "check.h"
#include "cmd/edits.h"
#include "expr/parser.h"
#include "impexp/file_format.h"

namespace fs = std::filesystem;
using book::sheet;
using book::workbook;
using val::cell_pos;

namespace {

cell_pos at(const char *name) {
    return *val::parse_cell_name(name);
}

fs::path scratch(const char *name) {
    return fs::temp_directory_path() / name;
}

void test_round_trip() {
    const fs::path path = scratch("isaac-round.gnumeric");
    {
        workbook book;
        sheet &s = book.add_sheet("First");
        book.add_sheet("Se\xC4\x8D & <b>");
        book::store_entry(s, at("A1"), "12.5");
        book::store_entry(s, at("A2"), "text & <more>");
        book::store_entry(s, at("A3"), "=SUM(A1:A1)*2");
        book::store_entry(s, at("A4"), "TRUE");
        book::store_entry(s, at("A5"), "#N/A");
        book::store_entry(s, at("B1"), "='Se\xC4\x8D & <b>'!A1+1");
        book::style_patch patch;
        patch.bold = true;
        patch.fore = util::color{10, 20, 30};
        patch.horizontal = book::h_align::center;
        patch.format = "0.00";
        patch.borders[0] = book::border_edge{book::border_line::thin,
                                             {1, 2, 3}};
        s.apply_style(*val::parse_range_name("A1:A2"), patch);
        s.cols().set_points(1, 100, true);
        s.rows().set_points(4, 30, true);
        s.rows().set_hidden(6, true);
        s.merge(*val::parse_range_name("C1:D2"));
        s.show_grid = false;
        const expr::parse_pos where{&s, {0, 0}, &book};
        book.define_name("tax", expr::parse_formula("0.2", where).tree,
                         nullptr, {0, 0});
        book::store_entry(s, at("B2"), "=tax*10");
        s.set_array(*val::parse_range_name("E1:E2"),
                    expr::parse_formula("{1;2}*2", where).tree);
        book.recalculate();
        std::string error;
        CHECK(impexp::save_file(path, book, error));
    }
    workbook book;
    const impexp::import_result result = impexp::load_file(path, book);
    CHECK(result.ok);
    CHECK(book.sheet_count() == 2);
    CHECK(book.sheet_at(1).name() == "Se\xC4\x8D & <b>");
    sheet &s = book.sheet_at(0);
    CHECK(s.value_at(at("A1")).number() == 12.5);
    CHECK(s.value_at(at("A2")).string() == "text & <more>");
    CHECK(s.entry_text(at("A3")) == "=SUM(A1:A1)*2");
    CHECK(s.value_at(at("A3")).number() == 25);
    CHECK(s.value_at(at("A4")).is_bool());
    CHECK(s.value_at(at("A5")).is_error());
    CHECK(s.style_at(at("A1"))->bold);
    CHECK(s.style_at(at("A1"))->fore == (util::color{10, 20, 30}));
    CHECK(s.style_at(at("A2"))->horizontal == book::h_align::center);
    CHECK(s.style_at(at("A1"))->format == "0.00");
    CHECK(s.style_at(at("A1"))->borders[0].line == book::border_line::thin);
    CHECK(!s.style_at(at("A3"))->bold);
    CHECK(s.cols().points(1) == 100);
    CHECK(s.rows().points(4) == 30);
    CHECK(s.rows().is_hidden(6));
    CHECK(s.merged().size() == 1);
    CHECK(!s.show_grid);
    CHECK(s.value_at(at("B2")).number() == 2);
    CHECK(s.value_at(at("E2")).number() == 4);
    CHECK(s.entry_text(at("E2")) == "={1;2}*2");
    fs::remove(path);
}

void test_csv() {
    const fs::path path = scratch("isaac-in.csv");
    {
        std::ofstream out(path);
        out << "a,\"b,c\",\"d\"\"e\"\n1,2.5,=A2+B2\n\"line\none\",,x\n";
    }
    workbook book;
    CHECK(impexp::load_file(path, book).ok);
    sheet &s = book.sheet_at(0);
    CHECK(s.value_at(at("B1")).string() == "b,c");
    CHECK(s.value_at(at("C1")).string() == "d\"e");
    CHECK(s.value_at(at("C2")).number() == 3.5);
    CHECK(s.value_at(at("A3")).string() == "line\none");
    const fs::path output = scratch("isaac-out.csv");
    std::string error;
    CHECK(impexp::save_file(output, book, error));
    std::ifstream in(output);
    std::string first;
    std::getline(in, first);
    CHECK(first == "a,\"b,c\",\"d\"\"e\"");
    fs::remove(path);
    fs::remove(output);
}

void test_html() {
    workbook book;
    sheet &s = book.add_sheet("Sheet1");
    book::store_entry(s, at("A1"), "<x>");
    const fs::path path = scratch("isaac.html");
    std::string error;
    CHECK(impexp::save_file(path, book, error));
    std::ifstream in(path);
    const std::string text((std::istreambuf_iterator<char>(in)), {});
    CHECK(text.find("&lt;x&gt;") != std::string::npos);
    fs::remove(path);
}

void test_failures() {
    workbook book;
    CHECK(!impexp::load_file("/nonexistent/file.gnumeric", book).ok);
    const fs::path path = scratch("isaac-bad.gnumeric");
    {
        std::ofstream out(path);
        out << "<gnm:Workbook><gnm:Sheets>";
    }
    workbook other;
    CHECK(!impexp::load_file(path, other).ok);
    fs::remove(path);
    workbook writable;
    writable.add_sheet("Sheet1");
    std::string error;
    CHECK(!impexp::save_file("/nonexistent/dir/x.gnumeric", writable, error));
    CHECK(!error.empty());
}

void test_samples() {
    for (const char *name : {"formula-tests.gnumeric", "string-tests.gnumeric",
                             "loan.gnumeric", "invoice.gnumeric"}) {
        workbook book;
        const impexp::import_result result =
            impexp::load_file(fs::path(FIXTURES) / name, book);
        CHECK(result.ok);
        CHECK(book.sheet_count() > 0);
    }
    // Files from before version 1.0 nest the text and name X11 fonts.
    workbook old;
    CHECK(impexp::load_file(fs::path(FIXTURES) / "invoice.gnumeric", old).ok);
    bool blank_text = false;
    int cells = 0;
    old.sheet_at(0).for_each_stored([&](book::cell &stored) {
        ++cells;
        blank_text = blank_text ||
                     (stored.result.is_string() &&
                      stored.result.string().find('\n') == 0);
    });
    CHECK(cells > 20);
    CHECK(!blank_text);
    CHECK(old.sheet_at(0).style_at({0, 1})->font_name.find('-') ==
          std::string::npos);
    workbook book;
    CHECK(impexp::load_file(fs::path(FIXTURES) / "formula-tests.gnumeric", book).ok);
    sheet &s = book.sheet_at(0);
    CHECK(s.value_at(at("B4")).number() == 5);
    CHECK(s.value_at(at("C16")).number() == 4);
    CHECK(s.value_at(at("C17")).number() == 4);
    // What this build does not model comes back out.
    const fs::path path = scratch("isaac-sample.gnumeric");
    std::string error;
    CHECK(impexp::save_file(path, book, error));
    workbook again;
    CHECK(impexp::load_file(path, again).ok);
    CHECK(again.sheet_at(0).preserved_xml.find("PrintInformation") !=
          std::string::npos);
    CHECK(again.sheet_at(0).value_at(at("C17")).number() == 4);
    fs::remove(path);
}

void test_undo() {
    workbook book;
    sheet &s = book.add_sheet("Sheet1");
    book.add_sheet("Two");
    cmd::command_stack stack(book);
    const auto range = [](const char *name) {
        return *val::parse_range_name(name);
    };
    CHECK(!stack.modified());
    stack.run(cmd::set_entry(s, at("A1"), "5"));
    stack.run(cmd::set_entry(s, at("A2"), "=A1*2"));
    CHECK(s.value_at(at("A2")).number() == 10);
    CHECK(stack.modified());
    stack.run(cmd::set_entry(s, at("A1"), "6"));
    CHECK(s.value_at(at("A2")).number() == 12);
    stack.undo();
    CHECK(s.value_at(at("A2")).number() == 10);
    stack.redo();
    CHECK(s.value_at(at("A2")).number() == 12);
    book::style_patch bold;
    bold.bold = true;
    stack.run(cmd::format_cells(s, {range("A1:A2")}, bold, "Bold"));
    CHECK(s.style_at(at("A1"))->bold);
    stack.undo();
    CHECK(!s.style_at(at("A1"))->bold);
    stack.run(cmd::clear_cells(s, {range("A1")}, cmd::clear_kind::contents));
    CHECK(s.value_at(at("A2")).number() == 0);
    stack.undo();
    CHECK(s.value_at(at("A2")).number() == 12);
    // Rows: references follow and come back.
    stack.run(cmd::change_rows(s, 0, 2, true));
    CHECK(s.entry_text(at("A4")) == "=A3*2");
    stack.undo();
    CHECK(s.entry_text(at("A2")) == "=A1*2");
    CHECK(s.value_at(at("A2")).number() == 12);
    stack.run(cmd::change_rows(s, 0, 1, false));
    CHECK(s.entry_text(at("A1")) == "=#REF!*2");
    stack.undo();
    CHECK(s.entry_text(at("A2")) == "=A1*2");
    CHECK(s.value_at(at("A1")).number() == 6);
    CHECK(s.value_at(at("A2")).number() == 12);
    stack.run(cmd::change_cols(s, 0, 1, true));
    CHECK(s.entry_text(at("B2")) == "=B1*2");
    stack.undo();
    stack.run(cmd::resize(s, true, 0, 2, 99));
    CHECK(s.cols().points(1) == 99);
    stack.undo();
    CHECK(s.cols().points(1) == 48);
    stack.run(cmd::set_hidden(s, false, 3, 4, true));
    CHECK(s.rows().is_hidden(4));
    stack.undo();
    CHECK(!s.rows().is_hidden(4));
    stack.run(cmd::merge_cells(s, range("C1:D1"), true));
    CHECK(s.merged().size() == 1);
    stack.undo();
    CHECK(s.merged().empty());
    // Moving: readers follow.
    stack.run(cmd::move_range(s, range("A1"), s, at("D5")));
    CHECK(s.entry_text(at("A2")) == "=D5*2");
    stack.undo();
    CHECK(s.entry_text(at("A2")) == "=A1*2");
    CHECK(s.value_at(at("A1")).number() == 6);
    stack.run(cmd::paste(s, range("C3"),
                         book::copy_region(s, range("A1:A2")), {}));
    CHECK(s.entry_text(at("C4")) == "=C3*2");
    stack.undo();
    CHECK(s.find(at("C4")) == nullptr);
    stack.run(cmd::autofill(s, range("A1"), range("A1:A1")));
    stack.undo();
    stack.run(cmd::fill_entry(s, range("F1:F3"), "=ROW()*2"));
    CHECK(s.value_at(at("F3")).number() == 6);
    stack.undo();
    CHECK(s.find(at("F3")) == nullptr);
    // Sheets.
    stack.run(cmd::add_sheet(2, "Three"));
    CHECK(book.sheet_count() == 3);
    stack.undo();
    CHECK(book.sheet_count() == 2);
    book::store_entry(book.sheet_at(1), at("A1"), "9");
    stack.run(cmd::set_entry(s, at("G1"), "=Two!A1+1"));
    CHECK(s.value_at(at("G1")).number() == 10);
    stack.run(cmd::remove_sheet(1));
    CHECK(s.value_at(at("G1")).is_error());
    stack.undo();
    CHECK(book.sheet_count() == 2);
    CHECK(s.value_at(at("G1")).number() == 10);
    CHECK(s.entry_text(at("G1")) == "=Two!A1+1");
    book::store_entry(book.sheet_at(1), at("A1"), "19");
    book.recalculate();
    CHECK(s.value_at(at("G1")).number() == 20);
    stack.redo();
    CHECK(s.value_at(at("G1")).is_error());
    stack.undo();
    CHECK(s.value_at(at("G1")).number() == 20);
    stack.run(cmd::rename_sheet(1, "Renamed"));
    CHECK(s.entry_text(at("G1")) == "=Renamed!A1+1");
    stack.undo();
    CHECK(s.entry_text(at("G1")) == "=Two!A1+1");
    stack.run(cmd::move_sheet(1, 0));
    CHECK(book.sheet_at(0).name() == "Two");
    stack.undo();
    CHECK(book.sheet_at(0).name() == "Sheet1");
    // The saved state is remembered through undo.
    stack.mark_saved();
    CHECK(!stack.modified());
    stack.run(cmd::set_entry(s, at("A5"), "1"));
    CHECK(stack.modified());
    stack.undo();
    CHECK(!stack.modified());
    CHECK(stack.can_redo());
    CHECK(stack.redo_name() == "Set cell");
}

} // namespace

int main() {
    test_round_trip();
    test_csv();
    test_html();
    test_failures();
    test_samples();
    test_undo();
    return testing::summary("impexp");
}
