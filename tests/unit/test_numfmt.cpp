//
// Number formats, General, dates and entry recognition.
//
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include <cmath>
#include <string>

#include "check.h"
#include "numfmt/date.h"
#include "numfmt/format.h"
#include "numfmt/match.h"
#include "util/gzip.h"
#include "util/text.h"

namespace {

// Render a number through a format string.
std::string show(const char *format, double value) {
    return numfmt::number_format::get(format)->render(value).text;
}

void test_dates() {
    using namespace numfmt;
    CHECK(serial_from_date(1900, 1, 1) == 1);
    CHECK(serial_from_date(1900, 2, 28) == 59);
    CHECK(serial_from_date(1900, 3, 1) == 61);
    CHECK(serial_from_date(2024, 1, 1) == 45292);
    CHECK(serial_from_date(2024, 14, 1) == serial_from_date(2025, 2, 1));
    CHECK(serial_from_date(2024, 0, 1) == serial_from_date(2023, 12, 1));
    CHECK((*date_from_serial(60) == civil_date{1900, 2, 29}));
    CHECK((*date_from_serial(61) == civil_date{1900, 3, 1}));
    CHECK((*date_from_serial(45292) == civil_date{2024, 1, 1}));
    CHECK(weekday_from_serial(45292) == 1); // A Monday.
    const civil_time noon = time_from_serial(45292.5);
    CHECK(noon.hour == 12 && noon.minute == 0);
}

void test_number_formats() {
    CHECK(show("0", 3.6) == "4");
    CHECK(show("0.00", 3.14159) == "3.14");
    CHECK(show("#,##0", 1234567) == "1,234,567");
    CHECK(show("#,##0.00", 1234.5) == "1,234.50");
    CHECK(show("0.00", -2.5) == "-2.50");
    CHECK(show("0%", 0.25) == "25%");
    CHECK(show("0.00%", 0.1234) == "12.34%");
    CHECK(show("$#,##0.00", 1234.5) == "$1,234.50");
    CHECK(show("0.00E+00", 12345) == "1.23E+04");
    CHECK(show("##0.0E+0", 12345) == "12.3E+3");
    CHECK(show("0.0#", 1.5) == "1.5");
    CHECK(show("0.0#", 1.234) == "1.23");
    CHECK(show("#", 0) == "");
    CHECK(show("000", 7) == "007");
    CHECK(show("#,##0,", 1234567) == "1,235");
    CHECK(show("0.00;(0.00)", -2.5) == "(2.50)");
    CHECK(show("0;-0;\"zero\"", 0) == "zero");
    CHECK(show("[>100]\"big\";\"small\"", 200) == "big");
    CHECK(show("[>100]\"big\";\"small\"", 50) == "small");
    CHECK(show("# ?/?", 1.5) == "1 1/2");
    CHECK(show("# \?\?/16", 2.25) == "2  4/16");
    CHECK(show("\"Q\"0", 3) == "Q3");
    CHECK(show("0_)", 3) == "3 ");
    const auto red =
        numfmt::number_format::get("[Red]0")->render(1);
    CHECK(red.color.has_value() && red.color->r == 255);
}

void test_date_formats() {
    const double serial = 45292.75;
    CHECK(show("yyyy-mm-dd", serial) == "2024-01-01");
    CHECK(show("m/d/yy", serial) == "1/1/24");
    CHECK(show("d mmm yyyy", serial) == "1 Jan 2024");
    CHECK(show("dddd, mmmm d", serial) == "Monday, January 1");
    CHECK(show("h:mm", serial) == "18:00");
    CHECK(show("h:mm AM/PM", serial) == "6:00 PM");
    CHECK(show("hh:mm:ss", 0.5) == "12:00:00");
    CHECK(show("[h]:mm", 1.5) == "36:00");
    CHECK(show("mm:ss", 0.0006944444444) == "01:00");
    using numfmt::format_family;
    CHECK(numfmt::number_format::get("yyyy")->family() ==
          format_family::date);
    CHECK(numfmt::number_format::get("h:mm")->family() ==
          format_family::time);
    CHECK(numfmt::number_format::get("0.00%")->family() ==
          format_family::percent);
    CHECK(numfmt::number_format::get("$0.00")->family() ==
          format_family::currency);
    CHECK(numfmt::number_format::get("@")->family() == format_family::text);
    CHECK(numfmt::number_format::get("General")->is_general());
}

void test_general() {
    using numfmt::render_general;
    CHECK(render_general(0) == "0");
    CHECK(render_general(1.5) == "1.5");
    CHECK(render_general(1234567) == "1234567");
    CHECK(render_general(0.1 + 0.2) == "0.3");
    CHECK(render_general(-2.5) == "-2.5");
    CHECK(render_general(1e20) == "1E+20");
    CHECK(render_general(1234.5678, 6) == "1234.6");
    CHECK(render_general(1234567890.0, 8) == "1.23E+09");
    CHECK(render_general(1.0 / 3.0, 6) == "0.3333");
}

void test_text_section() {
    const auto format = numfmt::number_format::get("0;0;0;\"[\"@\"]\"");
    CHECK(format->render_text("x").text == "[x]");
    CHECK(numfmt::number_format::get("0")->render_text("x").text == "x");
}

void test_match() {
    using numfmt::match_number;
    CHECK(match_number("12")->value == 12);
    CHECK(match_number("12")->format.empty());
    CHECK(match_number("-1.5e3")->value == -1500);
    CHECK(match_number(".5")->value == 0.5);
    CHECK(match_number("1,234")->value == 1234);
    CHECK(match_number("1,234")->format == "#,##0");
    CHECK(!match_number("1,23"));
    CHECK(match_number("50%")->value == 0.5);
    CHECK(match_number("50%")->format == "0%");
    CHECK(match_number("$1,234.50")->value == 1234.5);
    CHECK(match_number("$1,234.50")->format == "$#,##0.00");
    CHECK(match_number("(100)")->value == -100);
    CHECK(match_number("1 1/2")->value == 1.5);
    CHECK(match_number("12/25/2024")->value == 45651);
    CHECK(match_number("12/25/2024")->format == "m/d/yyyy");
    CHECK(match_number("2024-12-25")->value == 45651);
    CHECK(match_number("25-Dec-2024")->value == 45651);
    CHECK(match_number("Dec 25, 2024")->value == 45651);
    CHECK(match_number("12:30")->value == 0.5208333333333334);
    CHECK(match_number("1:30 PM")->format == "h:mm AM/PM");
    CHECK(match_number("1:30 PM")->value == 13.5 / 24);
    CHECK(match_number("12/25/2024 12:00")->value == 45651.5);
    CHECK(!match_number("hello"));
    CHECK(!match_number("12abc"));
    CHECK(!match_number("13/45/2024"));
    CHECK(!match_number(""));
}

void test_gzip() {
    std::string data;
    for (int i = 0; i < 5000; ++i)
        data += "<cell row=\"" + std::to_string(i) + "\">abc</cell>\n";
    const std::string packed = util::gzip(data);
    CHECK(packed.size() < data.size() / 3);
    CHECK(util::is_gzip(packed));
    const auto back = util::gunzip(packed);
    CHECK(back.has_value() && *back == data);
    CHECK(util::gunzip(util::gzip("")).value_or("x").empty());
    std::string damaged = packed;
    damaged[damaged.size() / 2] ^= 0x55;
    CHECK(util::gunzip(damaged).value_or("") != data);
}

void test_text() {
    CHECK(util::to_upper("gr\xC3\xBC\xC3\x9F") == "GR\xC3\x9C\xC3\x9F");
    CHECK(util::equal_nocase("Stra", "sTRA"));
    CHECK(util::equal_nocase("\xC5\xA0", "\xC5\xA1"));
    CHECK(util::shortest_double(0.1) == "0.1");
    CHECK(util::shortest_double(5) == "5");
    CHECK(util::char_count("a\xC3\xA9z") == 3);
    CHECK(util::byte_offset("a\xC3\xA9z", 2) == 3);
}

} // namespace

int main() {
    test_dates();
    test_number_formats();
    test_date_formats();
    test_general();
    test_text_section();
    test_match();
    test_gzip();
    test_text();
    return testing::summary("numfmt");
}
