//
// The calculation engine: parsing, printing, recalculation, references
// across structural edits, cycles, arrays and names.
//
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include <cmath>
#include <string>

#include "book/fill.h"
#include "book/region.h"
#include "book/workbook.h"
#include "check.h"
#include "expr/parser.h"

using book::sheet;
using book::store_entry;
using book::workbook;
using val::cell_pos;
using val::error_code;
using val::value;

namespace {

// Return a position from its name.
cell_pos at(const char *name) {
    return *val::parse_cell_name(name);
}

// Store an entry by cell name.
void put(sheet &where, const char *name, const char *text) {
    store_entry(where, at(name), text);
}

// Evaluate a formula typed at a scratch cell.
value calc(sheet &where, const char *formula) {
    const cell_pos scratch{200, 60000};
    store_entry(where, scratch, formula);
    where.book().recalculate();
    return where.value_at(scratch);
}

// Return a formula's result as a number, or NaN.
double num(sheet &where, const char *formula) {
    const value result = calc(where, formula);
    return result.is_number() ? result.number() : NAN;
}

// Return a formula's result as text.
std::string text(sheet &where, const char *formula) {
    return calc(where, formula).to_text();
}

// Report whether a formula yields an error.
bool err(sheet &where, const char *formula, error_code code) {
    const value result = calc(where, formula);
    return result.is_error() && result.error() == code;
}

// Compare within a tolerance.
bool near(double a, double b, double tolerance = 1e-9) {
    return std::fabs(a - b) <= tolerance;
}

void test_operators() {
    workbook book;
    sheet &s = book.add_sheet("Sheet1");
    CHECK(num(s, "=1+2*3") == 7);
    CHECK(num(s, "=(1+2)*3") == 9);
    CHECK(num(s, "=2^3^2") == 512);
    CHECK(num(s, "=-2^2") == 4);
    CHECK(num(s, "=2^-1") == 0.5);
    CHECK(num(s, "=50%") == 0.5);
    CHECK(num(s, "=10%*5") == 0.5);
    CHECK(text(s, "=\"a\"&\"b\"") == "ab");
    CHECK(text(s, "=1&2") == "12");
    CHECK(text(s, "=1=1") == "TRUE");
    CHECK(text(s, "=1<>1") == "FALSE");
    CHECK(text(s, "=\"abc\"=\"ABC\"") == "TRUE");
    CHECK(text(s, "=1<\"a\"") == "TRUE");
    CHECK(text(s, "=\"a\"<TRUE") == "TRUE");
    CHECK(num(s, "=\"5\"+1") == 6);
    CHECK(num(s, "=TRUE+1") == 2);
    CHECK(err(s, "=\"x\"+1", error_code::value));
    CHECK(err(s, "=1/0", error_code::div0));
    CHECK(err(s, "=0^0", error_code::num));
    CHECK(err(s, "=(-1)^0.5", error_code::num));
    CHECK(err(s, "=#REF!+1", error_code::ref));
    CHECK(err(s, "=NOSUCHNAME", error_code::name));
    CHECK(err(s, "=NOSUCHFUNC(1)", error_code::name));
    CHECK(num(s, "=SUM({1,2;3,4})") == 10);
    CHECK(text(s, "=\"say \\\"hi\\\"\"") == "say \"hi\"");
}

void test_printing() {
    workbook book;
    sheet &s = book.add_sheet("Sheet1");
    sheet &other = book.add_sheet("My Data");
    const auto round = [&](const char *formula) {
        store_entry(s, {3, 4}, formula);
        return s.entry_text({3, 4});
    };
    CHECK(round("=A1+B$2*$C3") == "=A1+B$2*$C3");
    CHECK(round("=sum(a1:b2)") == "=SUM(A1:B2)");
    CHECK(round("='My Data'!A1") == "='My Data'!A1");
    CHECK(round("=A:C") == "=A:C");
    CHECK(round("=SUM(1:3)") == "=SUM(1:3)");
    CHECK(round("=\"a\\\"b\"") == "=\"a\\\"b\"");
    CHECK(round("=(1+2)*3") == "=(1+2)*3");
    CHECK(round("=-A1") == "=-A1");
    CHECK(round("=A1%") == "=A1%");
    CHECK(round("={1,2;3,4}") == "={1,2;3,4}");
    CHECK(round("=foo(1)") == "=foo(1)");
    CHECK(round("=A1>=B2") == "=A1>=B2");
    CHECK(round("=A1:B2 B1:C3") == "=A1:B2 B1:C3");
    (void)other;
}

void test_recalculation() {
    workbook book;
    sheet &s = book.add_sheet("Sheet1");
    put(s, "A1", "1");
    put(s, "A2", "=A1+1");
    put(s, "A3", "=A2*2");
    put(s, "B1", "=SUM(A1:A3)");
    book.recalculate();
    CHECK(s.value_at(at("A3")).number() == 4);
    CHECK(s.value_at(at("B1")).number() == 7);
    put(s, "A1", "10");
    book.recalculate();
    CHECK(s.value_at(at("A3")).number() == 22);
    CHECK(s.value_at(at("B1")).number() == 43);
    s.clear_cell(at("A1"));
    book.recalculate();
    CHECK(s.value_at(at("A2")).number() == 1);
    // A cell in a range that did not exist when the formula was made.
    put(s, "C1", "=SUM(D1:D10)");
    book.recalculate();
    CHECK(s.value_at(at("C1")).number() == 0);
    put(s, "D5", "7");
    book.recalculate();
    CHECK(s.value_at(at("C1")).number() == 7);
    // A cross-sheet read.
    sheet &t = book.add_sheet("Two");
    put(t, "A1", "=Sheet1!D5*2");
    book.recalculate();
    CHECK(t.value_at(at("A1")).number() == 14);
    put(s, "D5", "8");
    book.recalculate();
    CHECK(t.value_at(at("A1")).number() == 16);
}

void test_long_chain() {
    workbook book;
    sheet &s = book.add_sheet("Sheet1");
    s.set_constant({0, 0}, value(1.0));
    const expr::parse_pos where{&s, {0, 1}, &book};
    const expr::node_ptr next = expr::parse_formula("A1+1", where).tree;
    for (int row = 1; row < 65536; ++row)
        s.set_formula({0, row}, next);
    book.recalculate();
    CHECK(s.value_at({0, 65535}).number() == 65536);
    s.set_constant({0, 0}, value(2.0));
    book.recalculate();
    CHECK(s.value_at({0, 65535}).number() == 65537);
    // Reading the far end first must not recurse down the chain.
    s.set_constant({0, 0}, value(3.0));
    CHECK(s.value_at({0, 65535}).number() == 65538);
}

void test_running_total() {
    workbook book;
    sheet &s = book.add_sheet("Sheet1");
    for (int row = 0; row < 5000; ++row)
        s.set_constant({0, row}, value(1.0));
    const expr::parse_pos where{&s, {1, 0}, &book};
    const expr::node_ptr total = expr::parse_formula("SUM(A$1:A1)", where).tree;
    for (int row = 0; row < 5000; ++row)
        s.set_formula({1, row}, total);
    book.recalculate();
    CHECK(s.value_at({1, 4999}).number() == 5000);
}

void test_cycles() {
    workbook book;
    sheet &s = book.add_sheet("Sheet1");
    put(s, "A1", "=A2/2+1");
    put(s, "A2", "=A1/2+1");
    put(s, "A3", "=A1+100");
    book.recalculate();
    // The fixed point of x = x/2 + 1 is 2.
    CHECK(near(s.value_at(at("A1")).number(), 2, 0.01));
    CHECK(near(s.value_at(at("A2")).number(), 2, 0.01));
    CHECK(near(s.value_at(at("A3")).number(), 102, 0.01));
    put(s, "B1", "=B1+1");
    book.recalculate();
    CHECK(s.value_at(at("B1")).is_number());
    workbook off;
    off.iteration_enabled = false;
    sheet &t = off.add_sheet("Sheet1");
    put(t, "A1", "=A1+1");
    off.recalculate();
    CHECK(t.value_at(at("A1")).is_number());
}

void test_volatile() {
    workbook book;
    sheet &s = book.add_sheet("Sheet1");
    put(s, "A1", "=RAND()");
    put(s, "A2", "=A1*1");
    book.recalculate();
    const double first = s.value_at(at("A2")).number();
    book.recalculate();
    CHECK(s.value_at(at("A2")).number() != first);
    CHECK(s.value_at(at("A2")).number() == s.value_at(at("A1")).number());
}

void test_structure() {
    workbook book;
    sheet &s = book.add_sheet("Sheet1");
    sheet &t = book.add_sheet("Other");
    put(s, "A1", "1");
    put(s, "A2", "2");
    put(s, "A3", "=SUM(A1:A2)");
    put(s, "B3", "=A1+A2");
    put(t, "A1", "=Sheet1!A2");
    book.recalculate();
    book.insert_rows(s, 1, 1);
    book.recalculate();
    CHECK(s.entry_text(at("A4")) == "=SUM(A1:A3)");
    CHECK(s.entry_text(at("B4")) == "=A1+A3");
    CHECK(t.entry_text(at("A1")) == "=Sheet1!A3");
    CHECK(s.value_at(at("A4")).number() == 3);
    put(s, "A2", "10");
    book.recalculate();
    CHECK(s.value_at(at("A4")).number() == 13);
    book.delete_rows(s, 2, 1);
    book.recalculate();
    CHECK(s.entry_text(at("A3")) == "=SUM(A1:A2)");
    CHECK(s.entry_text(at("B3")) == "=A1+#REF!");
    CHECK(t.entry_text(at("A1")) == "=#REF!");
    CHECK(s.value_at(at("A3")).number() == 11);
    book.insert_cols(s, 0, 2);
    book.recalculate();
    CHECK(s.entry_text(at("C3")) == "=SUM(C1:C2)");
    book.delete_cols(s, 0, 1);
    book.recalculate();
    CHECK(s.entry_text(at("B3")) == "=SUM(B1:B2)");
    CHECK(s.value_at(at("B3")).number() == 11);
    put(s, "E1", "=SUM(A:A)");
    book.delete_rows(s, 0, 1);
    book.recalculate();
    CHECK(s.entry_text(at("E1")).empty());
}

void test_sheet_removal() {
    workbook book;
    sheet &s = book.add_sheet("Sheet1");
    sheet &t = book.add_sheet("Gone");
    put(t, "A1", "5");
    put(s, "A1", "=Gone!A1+1");
    book.recalculate();
    CHECK(s.value_at(at("A1")).number() == 6);
    CHECK(book.remove_sheet(1));
    book.recalculate();
    CHECK(s.value_at(at("A1")).is_error());
    CHECK(!book.remove_sheet(0));
}

void test_names() {
    workbook book;
    sheet &s = book.add_sheet("Sheet1");
    put(s, "A1", "4");
    put(s, "B1", "=double*2");
    book.recalculate();
    CHECK(s.value_at(at("B1")).is_error());
    const expr::parse_pos where{&s, {0, 0}, &book};
    book.define_name("double", expr::parse_formula("$A$1*2", where).tree,
                     nullptr, {0, 0});
    book.recalculate();
    CHECK(s.value_at(at("B1")).number() == 16);
    put(s, "A1", "5");
    book.recalculate();
    CHECK(s.value_at(at("B1")).number() == 20);
    book.define_name("rate", expr::parse_formula("0.5", where).tree, nullptr,
                     {0, 0});
    CHECK(num(s, "=rate*4") == 2);
    book.define_name("data", expr::parse_formula("$A$1:$A$3", where).tree,
                     nullptr, {0, 0});
    put(s, "A2", "1");
    CHECK(num(s, "=SUM(data)") == 6);
}

void test_arrays() {
    workbook book;
    sheet &s = book.add_sheet("Sheet1");
    put(s, "A1", "1");
    put(s, "A2", "2");
    put(s, "A3", "3");
    put(s, "B1", "10");
    put(s, "B2", "20");
    put(s, "B3", "30");
    const expr::parse_pos where{&s, {3, 0}, &book};
    s.set_array(*val::parse_range_name("D1:D3"),
                expr::parse_formula("A1:A3*B1:B3", where).tree);
    book.recalculate();
    CHECK(s.value_at(at("D1")).number() == 10);
    CHECK(s.value_at(at("D2")).number() == 40);
    CHECK(s.value_at(at("D3")).number() == 90);
    put(s, "E1", "=SUM(D1:D3)");
    book.recalculate();
    CHECK(s.value_at(at("E1")).number() == 140);
    put(s, "A2", "3");
    book.recalculate();
    CHECK(s.value_at(at("D2")).number() == 60);
    CHECK(s.value_at(at("E1")).number() == 160);
    CHECK(s.entry_text(at("D2")) == "=A1:A3*B1:B3");
    // A function iterates in an array formula.
    const expr::parse_pos corner{&s, {5, 0}, &book};
    s.set_array(*val::parse_range_name("F1:F3"),
                expr::parse_formula("ABS(-A1:A3)", corner).tree);
    book.recalculate();
    CHECK(s.value_at(at("F3")).number() == 3);
    s.set_array(*val::parse_range_name("G1:H1"),
                expr::parse_formula("MMULT({1,2},{3,4;5,6})", where).tree);
    book.recalculate();
    CHECK(s.value_at(at("G1")).number() == 13);
    CHECK(s.value_at(at("H1")).number() == 16);
    CHECK(num(s, "=SUM(A1:A3*2)") == 14);
    CHECK(num(s, "=SUMPRODUCT(A1:A3,B1:B3)") == 160);
}

void test_implicit_intersection() {
    workbook book;
    sheet &s = book.add_sheet("Sheet1");
    put(s, "A1", "1");
    put(s, "A2", "2");
    put(s, "A3", "3");
    put(s, "B2", "=A1:A3*10");
    book.recalculate();
    CHECK(s.value_at(at("B2")).number() == 20);
    put(s, "B9", "=A1:A3");
    book.recalculate();
    CHECK(s.value_at(at("B9")).is_error());
}

void test_copy_paste() {
    workbook book;
    sheet &s = book.add_sheet("Sheet1");
    put(s, "A1", "1");
    put(s, "A2", "2");
    put(s, "B1", "=A1*2");
    book.recalculate();
    const book::region lifted =
        book::copy_region(s, *val::parse_range_name("B1"));
    book::paste_region(s, at("B2"), lifted);
    book.recalculate();
    CHECK(s.entry_text(at("B2")) == "=A2*2");
    CHECK(s.value_at(at("B2")).number() == 4);
    book::paste_region(s, at("C5"), lifted, {true, true, true, false});
    book.recalculate();
    CHECK(s.entry_text(at("C5")) == "2");
    book.move_range(s, *val::parse_range_name("A1:A2"), s, at("E1"));
    book.recalculate();
    CHECK(s.entry_text(at("B1")) == "=E1*2");
    CHECK(s.entry_text(at("B2")) == "=E2*2");
    CHECK(s.value_at(at("E2")).number() == 2);
    CHECK(s.find(at("A1")) == nullptr);
    book::paste_text(s, at("H1"), "1\t2\n=H1+I1\tx\n");
    book.recalculate();
    CHECK(s.value_at(at("H2")).number() == 3);
    CHECK(s.value_at(at("I2")).string() == "x");
    CHECK(book::region_as_text(s, *val::parse_range_name("H1:I1")) ==
          "1\t2\n");
}

void test_fill() {
    workbook book;
    sheet &s = book.add_sheet("Sheet1");
    put(s, "A1", "1");
    put(s, "A2", "3");
    book::autofill(s, *val::parse_range_name("A1:A2"),
                   *val::parse_range_name("A1:A5"));
    CHECK(s.value_at(at("A5")).number() == 9);
    put(s, "B1", "Mon");
    book::autofill(s, *val::parse_range_name("B1"),
                   *val::parse_range_name("B1:B3"));
    CHECK(s.value_at(at("B3")).string() == "Wed");
    put(s, "C1", "Item 9");
    book::autofill(s, *val::parse_range_name("C1"),
                   *val::parse_range_name("C1:C2"));
    CHECK(s.value_at(at("C2")).string() == "Item 10");
    put(s, "D1", "=A1*2");
    book::autofill(s, *val::parse_range_name("D1"),
                   *val::parse_range_name("D1:D3"));
    CHECK(s.entry_text(at("D3")) == "=A3*2");
    put(s, "E1", "7");
    book::autofill(s, *val::parse_range_name("E1"),
                   *val::parse_range_name("E1:G1"));
    CHECK(s.value_at(at("G1")).number() == 7);
}

void test_sort_search() {
    workbook book;
    sheet &s = book.add_sheet("Sheet1");
    put(s, "A1", "3");
    put(s, "B1", "c");
    put(s, "A2", "1");
    put(s, "B2", "a");
    put(s, "A3", "2");
    put(s, "B3", "b");
    book::sort_rows(s, *val::parse_range_name("A1:B3"), {{0, true, false}});
    CHECK(s.value_at(at("A1")).number() == 1);
    CHECK(s.value_at(at("B3")).string() == "c");
    book::sort_rows(s, *val::parse_range_name("A1:B3"), {{0, false, false}});
    CHECK(s.value_at(at("B1")).string() == "c");
    const auto hit = book::find_text(s, at("A1"), "B", {});
    CHECK(hit && hit->pos == at("B2"));
    CHECK(book::replace_in_cell(s, at("B2"), "b", "zebra", {}));
    CHECK(s.value_at(at("B2")).string() == "zebra");
    CHECK(!book::find_text(s, at("A1"), "qqq", {}));
}

void test_styles() {
    workbook book;
    sheet &s = book.add_sheet("Sheet1");
    const auto base = s.style_at(at("B2"));
    book::style_patch bold;
    bold.bold = true;
    s.apply_style(*val::parse_range_name("B2:C3"), bold);
    CHECK(s.style_at(at("B2"))->bold);
    CHECK(s.style_at(at("C3"))->bold);
    CHECK(!s.style_at(at("D3"))->bold);
    CHECK(s.style_at(at("B2")) == s.style_at(at("C3")));
    bold.bold = false;
    s.apply_style(*val::parse_range_name("B2:C3"), bold);
    CHECK(s.style_at(at("B2")) == base);
    s.apply_style(*val::parse_range_name("A1:A5"), bold);
    book::style_patch red;
    red.fore = util::color{255, 0, 0};
    s.apply_style(*val::parse_range_name("A2:A3"), red);
    book.insert_rows(s, 2, 2);
    CHECK(s.style_at(at("A5"))->fore.r == 255);
    CHECK(s.style_at(at("A3"))->fore.r == 255);
    book.delete_rows(s, 0, 1);
    CHECK(s.style_at(at("A1"))->fore.r == 255);
    // Deleting every row or column leaves a usable sheet.
    book.delete_rows(s, 0, s.row_count());
    CHECK(s.style_at(at("A1")) != nullptr);
    CHECK(s.style_at({0, s.row_count() - 1}) != nullptr);
    book.delete_cols(s, 0, s.col_count());
    CHECK(s.style_at(at("B2")) != nullptr);
    s.apply_style(*val::parse_range_name("A1:A5"), red);
    int regions = 0;
    s.styles().for_each_region(
        book.styles().default_style(),
        [&](const val::cell_range &, const book::style_ptr &) { ++regions; });
    CHECK(regions >= 1);
}

void test_axis() {
    book::axis rows(65536, 12.75);
    rows.set_scale(96.0 / 72.0);
    CHECK(rows.pixels(0) == 17);
    CHECK(rows.offset(10) == 170);
    rows.set_points(5, 30, true);
    CHECK(rows.pixels(5) == 40);
    CHECK(rows.offset(5) == 85);
    CHECK(rows.offset(6) == 125);
    CHECK(rows.offset(7) == 142);
    CHECK(rows.index_at(84) == 4);
    CHECK(rows.index_at(85) == 5);
    CHECK(rows.index_at(124) == 5);
    CHECK(rows.index_at(125) == 6);
    CHECK(rows.index_at(1000000000) == 65535);
    rows.set_hidden(2, true);
    CHECK(rows.pixels(2) == 0);
    CHECK(rows.offset(3) == 34);
    CHECK(rows.index_at(34) == 3);
    rows.shift(1, 1);
    CHECK(rows.is_hidden(3));
    CHECK(rows.pixels(6) == 40);
    rows.shift(1, -1);
    CHECK(rows.is_hidden(2));
}

void test_entries() {
    workbook book;
    sheet &s = book.add_sheet("Sheet1");
    put(s, "A1", "'123");
    CHECK(s.value_at(at("A1")).is_string());
    CHECK(s.entry_text(at("A1")) == "'123");
    put(s, "A2", "TRUE");
    CHECK(s.value_at(at("A2")).is_bool());
    put(s, "A3", "#N/A");
    CHECK(s.value_at(at("A3")).is_error());
    put(s, "A4", "50%");
    CHECK(s.style_at(at("A4"))->format == "0%");
    CHECK(s.entry_text(at("A4")) == "50%");
    CHECK(book::display_text(s, at("A4")) == "50%");
    put(s, "A5", "12/25/2024");
    CHECK(book::display_text(s, at("A5")) == "12/25/2024");
    put(s, "A6", "=DATE(2024,1,1)");
    book.recalculate();
    CHECK(s.style_at(at("A6"))->format == "m/d/yyyy");
    CHECK(book::display_text(s, at("A6")) == "1/1/2024");
    put(s, "A7", "=1+");
    CHECK(s.value_at(at("A7")).is_string());
    put(s, "A8", "-5");
    CHECK(s.value_at(at("A8")).number() == -5);
    put(s, "A9", "-A1");
    CHECK(s.find(at("A9"))->has_formula());
    put(s, "A10", "hello");
    CHECK(s.entry_text(at("A10")) == "hello");
    book::style_patch text;
    text.format = "@";
    s.apply_style(val::cell_range::single(at("A11")), text);
    put(s, "A11", "=1+1");
    CHECK(s.value_at(at("A11")).is_string());
}

} // namespace

int main() {
    test_operators();
    test_printing();
    test_recalculation();
    test_long_chain();
    test_running_total();
    test_cycles();
    test_volatile();
    test_structure();
    test_sheet_removal();
    test_names();
    test_arrays();
    test_implicit_intersection();
    test_copy_paste();
    test_fill();
    test_sort_search();
    test_styles();
    test_axis();
    test_entries();
    return testing::summary("engine");
}
