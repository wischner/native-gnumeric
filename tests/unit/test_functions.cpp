//
// Spreadsheet functions against known results: the examples in
// Gnumeric's and Excel's documentation and hand-checked values.
//
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include <cmath>
#include <numbers>
#include <string>

#include "book/region.h"
#include "book/workbook.h"
#include "check.h"
#include "func/functions.h"
#include "func/mathfunc.h"

using book::sheet;
using book::workbook;
using val::cell_pos;
using val::error_code;
using val::value;

namespace {

// Holds a sheet with a small table for the tests to read.
class bench {
public:
    bench() : sheet_(book_.add_sheet("Sheet1")) {
        const char *rows[][4] = {
            {"Name", "Qty", "Price", "Date"},
            {"pear", "4", "1.5", "2024-01-15"},
            {"apple", "10", "0.5", "2024-02-20"},
            {"fig", "7", "3", "2024-03-25"},
            {"apple", "2", "0.75", "2024-04-30"},
        };
        for (int row = 0; row < 5; ++row)
            for (int col = 0; col < 4; ++col)
                book::store_entry(sheet_, {col, row}, rows[row][col]);
        book::store_entry(sheet_, {5, 0}, "Name");
        book::store_entry(sheet_, {5, 1}, "apple");
        book_.recalculate();
    }

    value run(const char *formula) {
        const cell_pos scratch{100, 1000};
        book::store_entry(sheet_, scratch, formula);
        book_.recalculate();
        return sheet_.value_at(scratch);
    }

private:
    workbook book_;
    sheet &sheet_;
};

bench *table = nullptr;

// Report whether a formula yields a number within tolerance.
bool eq(const char *formula, double expected, double tolerance = 1e-9) {
    const value result = table->run(formula);
    if (!result.is_number()) {
        std::cerr << formula << " -> " << result.to_text() << '\n';
        return false;
    }
    const bool good = std::fabs(result.number() - expected) <=
                      tolerance * std::max(1.0, std::fabs(expected));
    if (!good)
        std::cerr.precision(15), std::cerr << formula << " -> "
                                           << result.number() << " want "
                                           << expected << '\n';
    return good;
}

// Report whether a formula yields a text.
bool is(const char *formula, const char *expected) {
    const std::string result = table->run(formula).to_text();
    if (result != expected)
        std::cerr << formula << " -> " << result << " want " << expected
                  << '\n';
    return result == expected;
}

// Report whether a formula yields an error.
bool fails(const char *formula, error_code code) {
    const value result = table->run(formula);
    return result.is_error() && result.error() == code;
}

void test_math() {
    CHECK(eq("=SUM(1,2,3)", 6));
    CHECK(eq("=SUM(B2:B5)", 23));
    CHECK(eq("=SUM(A2:A5)", 0));
    CHECK(eq("=SUM(B2:B5,10)", 33));
    CHECK(eq("=PRODUCT(2,3,4)", 24));
    CHECK(eq("=ABS(-3)", 3));
    CHECK(eq("=INT(-2.5)", -3));
    CHECK(eq("=TRUNC(-2.5)", -2));
    CHECK(eq("=TRUNC(3.14159,2)", 3.14));
    CHECK(eq("=ROUND(2.5,0)", 3));
    CHECK(eq("=ROUND(-2.5,0)", -3));
    CHECK(eq("=ROUND(2.675,2)", 2.68));
    CHECK(eq("=ROUND(1234.5,-2)", 1200));
    CHECK(eq("=ROUNDUP(3.2,0)", 4));
    CHECK(eq("=ROUNDUP(-3.2,0)", -4));
    CHECK(eq("=ROUNDDOWN(3.9,0)", 3));
    CHECK(eq("=CEILING(2.1,0.5)", 2.5));
    CHECK(eq("=CEILING(4,2)", 4));
    CHECK(eq("=FLOOR(2.7,0.5)", 2.5));
    CHECK(eq("=MROUND(7,3)", 6));
    CHECK(eq("=EVEN(1.5)", 2));
    CHECK(eq("=EVEN(3)", 4));
    CHECK(eq("=EVEN(2)", 2));
    CHECK(eq("=ODD(1.5)", 3));
    CHECK(eq("=ODD(2)", 3));
    CHECK(eq("=ODD(3)", 3));
    CHECK(eq("=ODD(-2)", -3));
    CHECK(eq("=MOD(7,3)", 1));
    CHECK(eq("=MOD(-7,3)", 2));
    CHECK(eq("=MOD(7,-3)", -2));
    CHECK(fails("=MOD(1,0)", error_code::div0));
    CHECK(eq("=QUOTIENT(7,2)", 3));
    CHECK(eq("=POWER(2,10)", 1024));
    CHECK(eq("=SQRT(16)", 4));
    CHECK(fails("=SQRT(-1)", error_code::num));
    CHECK(eq("=EXP(0)", 1));
    CHECK(eq("=LN(1)", 0));
    CHECK(eq("=LOG(100)", 2));
    CHECK(eq("=LOG(8,2)", 3));
    CHECK(eq("=LOG10(1000)", 3));
    CHECK(eq("=FACT(5)", 120));
    CHECK(eq("=FACT(0)", 1));
    CHECK(eq("=FACTDOUBLE(6)", 48));
    CHECK(eq("=COMBIN(8,2)", 28));
    CHECK(eq("=COMBINA(4,3)", 20));
    CHECK(eq("=GCD(12,18)", 6));
    CHECK(eq("=LCM(4,6)", 12));
    CHECK(eq("=MULTINOMIAL(2,3,4)", 1260));
    CHECK(eq("=SIGN(-5)", -1));
    CHECK(eq("=PI()", std::numbers::pi));
    CHECK(eq("=DEGREES(PI())", 180));
    CHECK(eq("=RADIANS(180)", std::numbers::pi));
    CHECK(eq("=SIN(0)", 0));
    CHECK(eq("=COS(0)", 1));
    CHECK(eq("=ATAN2(1,1)", std::numbers::pi / 4));
    CHECK(eq("=SUMSQ(3,4)", 25));
    CHECK(eq("=SUMPRODUCT(B2:B5,C2:C5)", 33.5));
    CHECK(eq("=SUMX2MY2({2,3},{1,1})", 11));
    CHECK(eq("=SUMX2PY2({2,3},{1,1})", 15));
    CHECK(eq("=SUMXMY2({2,3},{1,1})", 5));
    CHECK(eq("=SERIESSUM(2,1,1,{1,1})", 6));
    CHECK(is("=ROMAN(1994)", "MCMXCIV"));
    CHECK(eq("=FIB(12)", 144));
    CHECK(eq("=COUNTIF(A2:A5,\"apple\")", 2));
    CHECK(eq("=COUNTIF(B2:B5,\">=4\")", 3));
    CHECK(eq("=COUNTIF(A2:A5,\"*p*\")", 3));
    CHECK(eq("=COUNTIF(A2:A5,\"?ig\")", 1));
    CHECK(eq("=SUMIF(A2:A5,\"apple\",B2:B5)", 12));
    CHECK(eq("=SUMIF(B2:B5,\">4\")", 17));
    CHECK(eq("=AVERAGEIF(A2:A5,\"apple\",B2:B5)", 6));
    CHECK(eq("=COUNTIFS(A2:A5,\"apple\",B2:B5,\">5\")", 1));
    CHECK(eq("=SUMIFS(B2:B5,A2:A5,\"apple\",C2:C5,\"<0.6\")", 10));
    CHECK(eq("=MAXIFS(B2:B5,A2:A5,\"apple\")", 10));
    CHECK(eq("=MINIFS(B2:B5,A2:A5,\"apple\")", 2));
    CHECK(eq("=SUBTOTAL(9,B2:B5)", 23));
    CHECK(eq("=SUBTOTAL(1,B2:B5)", 5.75));
    CHECK(eq("=SUBTOTAL(109,B2:B5)", 23));
    const double random = table->run("=RAND()").number();
    CHECK(random >= 0 && random < 1);
    const double between = table->run("=RANDBETWEEN(3,5)").number();
    CHECK(between >= 3 && between <= 5);
}

void test_stat() {
    CHECK(eq("=AVERAGE(B2:B5)", 5.75));
    CHECK(eq("=AVERAGE(1,2,3,4)", 2.5));
    CHECK(fails("=AVERAGE(A2:A5)", error_code::div0));
    CHECK(eq("=AVERAGEA(A1:B2)", 1));
    CHECK(eq("=COUNT(B1:B5)", 4));
    CHECK(eq("=COUNTA(A1:A5)", 5));
    CHECK(eq("=COUNTBLANK(A1:A6)", 1));
    CHECK(eq("=MAX(B2:B5)", 10));
    CHECK(eq("=MIN(B2:B5)", 2));
    CHECK(eq("=MAX(A2:A5)", 0));
    CHECK(eq("=MEDIAN(1,2,3,4)", 2.5));
    CHECK(eq("=MEDIAN(1,3,2)", 2));
    CHECK(eq("=MODE(1,2,2,3)", 2));
    CHECK(fails("=MODE(1,2,3)", error_code::na));
    CHECK(eq("=VAR(1,2,3,4,5)", 2.5));
    CHECK(eq("=VARP(1,2,3,4,5)", 2));
    CHECK(eq("=STDEV(1,2,3,4,5)", std::sqrt(2.5)));
    CHECK(eq("=STDEVP(1,2,3,4,5)", std::sqrt(2.0)));
    CHECK(eq("=DEVSQ(1,2,3)", 2));
    CHECK(eq("=AVEDEV(1,2,3)", 2.0 / 3));
    CHECK(eq("=GEOMEAN(2,8)", 4));
    CHECK(eq("=HARMEAN(1,2,4)", 12.0 / 7));
    CHECK(eq("=SKEW(3,4,5,2,3,4,5,6,4,7)", 0.359543071));
    CHECK(eq("=KURT(3,4,5,2,3,4,5,6,4,7)", -0.151799637));
    CHECK(eq("=LARGE(B2:B5,1)", 10));
    CHECK(eq("=SMALL(B2:B5,2)", 4));
    CHECK(eq("=PERCENTILE({1,2,3,4},0.3)", 1.9));
    CHECK(eq("=QUARTILE({1,2,4,7,8,9,10,12},1)", 3.5));
    CHECK(eq("=PERCENTRANK({1,2,3,4,5},2)", 0.25));
    CHECK(eq("=RANK(7,B2:B5)", 2));
    CHECK(eq("=RANK(7,B2:B5,1)", 3));
    CHECK(eq("=TRIMMEAN({1,2,3,4,100},0.4)", 3));
    CHECK(eq("=CORREL({1,2,3},{2,4,6})", 1));
    CHECK(eq("=PEARSON({1,2,3},{3,2,1})", -1));
    CHECK(eq("=RSQ({1,2,3},{1,2,3})", 1));
    CHECK(eq("=COVAR({1,2,3},{1,2,3})", 2.0 / 3));
    CHECK(eq("=SLOPE({2,4,6},{1,2,3})", 2));
    CHECK(eq("=INTERCEPT({3,5,7},{1,2,3})", 1));
    CHECK(eq("=FORECAST(4,{3,5,7},{1,2,3})", 9));
    CHECK(eq("=STEYX({1,2,3},{1,2,3})", 0));
    CHECK(eq("=INDEX(LINEST({3,5,7},{1,2,3}),1,1)", 2));
    CHECK(eq("=INDEX(LINEST({3,5,7},{1,2,3}),1,2)", 1));
    CHECK(eq("=INDEX(TREND({3,5,7},{1,2,3},{4}),1,1)", 9));
    CHECK(eq("=NORMSDIST(0)", 0.5));
    CHECK(eq("=NORMSDIST(1.96)", 0.97500210485178, 1e-10));
    CHECK(eq("=NORMDIST(42,40,1.5,TRUE)", 0.908788780274132, 1e-10));
    CHECK(eq("=NORMDIST(42,40,1.5,FALSE)", 0.109340049783996, 1e-10));
    CHECK(eq("=NORMSINV(0.975)", 1.95996398454005, 1e-9));
    CHECK(eq("=NORMINV(0.908789,40,1.5)", 42.0000002, 1e-6));
    CHECK(eq("=TDIST(1.96,10,2)", 0.0785, 1e-3));
    CHECK(eq("=TINV(0.05,10)", 2.22813885198627, 1e-8));
    CHECK(eq("=CHIDIST(18.307,10)", 0.0500006, 1e-5));
    CHECK(eq("=CHIINV(0.05,10)", 18.3070380532751, 1e-8));
    CHECK(eq("=FDIST(15.2068649,6,4)", 0.01, 1e-7));
    CHECK(eq("=FINV(0.01,6,4)", 15.2068649, 1e-6));
    CHECK(eq("=BINOMDIST(6,10,0.5,FALSE)", 0.205078125, 1e-10));
    CHECK(eq("=BINOMDIST(6,10,0.5,TRUE)", 0.828125, 1e-10));
    CHECK(eq("=POISSON(2,5,FALSE)", 0.0842243374718, 1e-10));
    CHECK(eq("=POISSON(2,5,TRUE)", 0.124652019483, 1e-10));
    CHECK(eq("=EXPONDIST(0.2,10,TRUE)", 0.864664716763387, 1e-10));
    CHECK(eq("=GAMMADIST(10.00001131,9,2,TRUE)", 0.068094, 1e-5));
    CHECK(eq("=GAMMAINV(0.068094,9,2)", 10.0000, 1e-4));
    CHECK(eq("=BETADIST(2,8,10,1,3)", 0.6854706, 1e-6));
    CHECK(eq("=BETAINV(0.6854706,8,10,1,3)", 2, 1e-5));
    CHECK(eq("=HYPGEOMDIST(1,4,8,20)", 0.363261, 1e-5));
    CHECK(eq("=NEGBINOMDIST(10,5,0.25)", 0.0550487, 1e-5));
    CHECK(eq("=WEIBULL(105,20,100,TRUE)", 0.929581, 1e-5));
    CHECK(eq("=LOGNORMDIST(4,3.5,1.2)", 0.0390836, 1e-5));
    CHECK(eq("=LOGINV(0.039084,3.5,1.2)", 4.0000252, 1e-4));
    CHECK(eq("=CONFIDENCE(0.05,2.5,50)", 0.692951912, 1e-8));
    CHECK(eq("=STANDARDIZE(42,40,1.5)", 1.33333333333, 1e-9));
    CHECK(eq("=FISHER(0.75)", 0.972955074527657, 1e-10));
    CHECK(eq("=FISHERINV(0.972955)", 0.75, 1e-5));
    CHECK(eq("=PERMUT(100,3)", 970200));
    CHECK(eq("=CRITBINOM(6,0.5,0.75)", 4));
    CHECK(eq("=PROB({0,1,2,3},{0.2,0.3,0.1,0.4},2)", 0.1));
    CHECK(eq("=PROB({0,1,2,3},{0.2,0.3,0.1,0.4},1,3)", 0.8));
    CHECK(eq("=TTEST({3,4,5,8,9,1,2,4,5},{6,19,3,2,14,4,5,17,1},2,1)",
             0.196016, 1e-5));
    CHECK(eq("=FTEST({6,7,9,15,21},{20,28,31,38,40})", 0.648317847, 1e-6));
    CHECK(eq("=INDEX(FREQUENCY({1,2,3,4,5},{2,4}),1,1)", 2));
    CHECK(eq("=INDEX(FREQUENCY({1,2,3,4,5},{2,4}),3,1)", 1));
}

void test_string() {
    CHECK(is("=LEFT(\"hello\",2)", "he"));
    CHECK(is("=LEFT(\"hello\")", "h"));
    CHECK(is("=RIGHT(\"hello\",2)", "lo"));
    CHECK(is("=MID(\"hello\",2,3)", "ell"));
    CHECK(is("=MID(\"hello\",9,3)", ""));
    CHECK(eq("=LEN(\"\xC4\x8D\x65\")", 2));
    CHECK(eq("=LENB(\"\xC4\x8D\x65\")", 3));
    CHECK(is("=UPPER(\"abc\")", "ABC"));
    CHECK(is("=LOWER(\"ABC\")", "abc"));
    CHECK(is("=PROPER(\"hello wORLD\")", "Hello World"));
    CHECK(is("=TRIM(\"  a   b  \")", "a b"));
    CHECK(is("=REPT(\"ab\",3)", "ababab"));
    CHECK(is("=REPLACE(\"abcdef\",2,3,\"X\")", "aXef"));
    CHECK(is("=SUBSTITUTE(\"a-b-c\",\"-\",\"+\")", "a+b+c"));
    CHECK(is("=SUBSTITUTE(\"a-b-c\",\"-\",\"+\",2)", "a-b+c"));
    CHECK(eq("=FIND(\"b\",\"abcb\")", 2));
    CHECK(eq("=FIND(\"b\",\"abcb\",3)", 4));
    CHECK(fails("=FIND(\"B\",\"abc\")", error_code::value));
    CHECK(eq("=SEARCH(\"B\",\"abc\")", 2));
    CHECK(eq("=SEARCH(\"?c\",\"abc\")", 2));
    CHECK(is("=EXACT(\"a\",\"A\")", "FALSE"));
    CHECK(is("=CHAR(65)", "A"));
    CHECK(eq("=CODE(\"A\")", 65));
    CHECK(is("=UNICHAR(269)", "\xC4\x8D"));
    CHECK(eq("=UNICODE(\"\xC4\x8D\")", 269));
    CHECK(is("=T(\"a\")", "a"));
    CHECK(is("=T(1)", ""));
    CHECK(is("=TEXT(1234.5,\"#,##0.00\")", "1,234.50"));
    CHECK(is("=TEXT(0.5,\"0%\")", "50%"));
    CHECK(is("=TEXT(45292,\"yyyy-mm-dd\")", "2024-01-01"));
    CHECK(eq("=VALUE(\"12.5\")", 12.5));
    CHECK(eq("=VALUE(\"50%\")", 0.5));
    CHECK(fails("=VALUE(\"abc\")", error_code::value));
    CHECK(eq("=NUMBERVALUE(\"1.234,5\",\",\",\".\")", 1234.5));
    CHECK(is("=FIXED(1234.567,1)", "1,234.6"));
    CHECK(is("=FIXED(1234.567,1,TRUE)", "1234.6"));
    CHECK(is("=DOLLAR(1234.567,2)", "$1,234.57"));
    CHECK(is("=DOLLAR(-5,2)", "($5.00)"));
    CHECK(is("=CONCATENATE(\"a\",1,TRUE)", "a1TRUE"));
    CHECK(is("=CONCAT(A2:A3)", "pearapple"));
    CHECK(is("=TEXTJOIN(\",\",TRUE,\"a\",\"\",\"b\")", "a,b"));
    CHECK(is("=TEXTJOIN(\",\",FALSE,\"a\",\"\",\"b\")", "a,,b"));
}

void test_logical() {
    CHECK(is("=AND(TRUE,FALSE)", "FALSE"));
    CHECK(is("=AND(1,2)", "TRUE"));
    CHECK(is("=OR(FALSE,1)", "TRUE"));
    CHECK(is("=XOR(TRUE,TRUE)", "FALSE"));
    CHECK(is("=NOT(FALSE)", "TRUE"));
    CHECK(is("=TRUE()", "TRUE"));
    CHECK(eq("=IF(1>0,5,6)", 5));
    CHECK(eq("=IF(1<0,5,6)", 6));
    CHECK(is("=IF(1<0,5)", "FALSE"));
    CHECK(eq("=IF(TRUE,1,1/0)", 1));
    CHECK(eq("=IFERROR(1/0,9)", 9));
    CHECK(eq("=IFERROR(2,9)", 2));
    CHECK(eq("=IFNA(NA(),3)", 3));
    CHECK(fails("=IFNA(1/0,3)", error_code::div0));
    CHECK(eq("=IFS(FALSE,1,TRUE,2)", 2));
    CHECK(eq("=SWITCH(2,1,10,2,20)", 20));
    CHECK(eq("=SWITCH(3,1,10,2,20,99)", 99));
}

void test_lookup() {
    CHECK(eq("=VLOOKUP(\"fig\",A2:C5,2,FALSE)", 7));
    CHECK(is("=VLOOKUP(\"fig\",A2:C5,1,FALSE)", "fig"));
    CHECK(fails("=VLOOKUP(\"none\",A2:C5,2,FALSE)", error_code::na));
    CHECK(eq("=VLOOKUP(2.5,{1,10;2,20;3,30},2)", 20));
    CHECK(eq("=VLOOKUP(5,{1,10;2,20;3,30},2)", 30));
    CHECK(fails("=VLOOKUP(0,{1,10;2,20},2)", error_code::na));
    CHECK(eq("=HLOOKUP(\"Qty\",A1:C5,3,FALSE)", 10));
    CHECK(eq("=MATCH(7,B2:B5,0)", 3));
    CHECK(eq("=MATCH(5,{1,4,7})", 2));
    CHECK(eq("=MATCH(5,{7,4,1},-1)", 1));
    CHECK(eq("=MATCH(\"a*\",A2:A5,0)", 2));
    CHECK(eq("=INDEX(B2:C5,2,2)", 0.5));
    CHECK(eq("=INDEX(B2:B5,3)", 7));
    CHECK(eq("=SUM(INDEX(B2:C5,0,2))", 5.75));
    CHECK(eq("=LOOKUP(2.5,{1,2,3},{10,20,30})", 20));
    CHECK(eq("=XLOOKUP(\"fig\",A2:A5,B2:B5)", 7));
    CHECK(is("=XLOOKUP(\"x\",A2:A5,B2:B5,\"none\")", "none"));
    CHECK(eq("=COLUMN(C4)", 3));
    CHECK(eq("=ROW(C4)", 4));
    CHECK(eq("=ROW()", 1001));
    CHECK(eq("=COLUMNS(A1:C4)", 3));
    CHECK(eq("=ROWS(A1:C4)", 4));
    CHECK(is("=ADDRESS(2,3)", "$C$2"));
    CHECK(is("=ADDRESS(2,3,4)", "C2"));
    CHECK(is("=ADDRESS(2,3,1,TRUE,\"Sheet 2\")", "'Sheet 2'!$C$2"));
    CHECK(eq("=INDIRECT(\"B3\")", 10));
    CHECK(eq("=SUM(INDIRECT(\"B2:B3\"))", 14));
    CHECK(eq("=OFFSET(A1,2,1)", 10));
    CHECK(eq("=SUM(OFFSET(B2,0,0,2,1))", 14));
    CHECK(eq("=CHOOSE(2,5,6,7)", 6));
    CHECK(fails("=CHOOSE(9,5,6)", error_code::value));
    CHECK(eq("=INDEX(TRANSPOSE({1,2,3}),2,1)", 2));
    CHECK(is("=HYPERLINK(\"http://x\",\"link\")", "link"));
    CHECK(is("=FORMULATEXT(A1)", "#N/A"));
}

void test_date() {
    CHECK(eq("=DATE(2024,1,1)", 45292));
    CHECK(eq("=DATE(2024,13,1)", 45658));
    CHECK(eq("=YEAR(45292)", 2024));
    CHECK(eq("=MONTH(45292)", 1));
    CHECK(eq("=DAY(45292)", 1));
    CHECK(eq("=DATEVALUE(\"2024-01-01\")", 45292));
    CHECK(eq("=TIME(12,0,0)", 0.5));
    CHECK(eq("=TIMEVALUE(\"12:00\")", 0.5));
    CHECK(eq("=HOUR(0.5)", 12));
    CHECK(eq("=MINUTE(0.75)", 0));
    CHECK(eq("=SECOND(0.000011574)", 1));
    CHECK(eq("=WEEKDAY(45292)", 2));
    CHECK(eq("=WEEKDAY(45292,2)", 1));
    CHECK(eq("=WEEKDAY(45292,3)", 0));
    CHECK(eq("=WEEKNUM(45292)", 1));
    CHECK(eq("=ISOWEEKNUM(DATE(2021,1,3))", 53));
    CHECK(eq("=EDATE(DATE(2024,1,31),1)", 45351));
    CHECK(eq("=EOMONTH(DATE(2024,1,15),1)", 45351));
    CHECK(eq("=DAYS(DATE(2024,3,1),DATE(2024,1,1))", 60));
    CHECK(eq("=DAYS360(DATE(2024,1,1),DATE(2024,3,1))", 60));
    CHECK(eq("=DATEDIF(DATE(2020,1,1),DATE(2024,3,5),\"y\")", 4));
    CHECK(eq("=DATEDIF(DATE(2020,1,1),DATE(2024,3,5),\"m\")", 50));
    CHECK(eq("=DATEDIF(DATE(2020,1,1),DATE(2024,3,5),\"ym\")", 2));
    CHECK(eq("=DATEDIF(DATE(2020,1,1),DATE(2024,3,5),\"md\")", 4));
    CHECK(eq("=YEARFRAC(DATE(2024,1,1),DATE(2024,7,1))", 0.5));
    CHECK(eq("=NETWORKDAYS(DATE(2024,1,1),DATE(2024,1,12))", 10));
    CHECK(eq("=NETWORKDAYS(DATE(2024,1,1),DATE(2024,1,12),{45293})", 9));
    CHECK(eq("=WORKDAY(DATE(2024,1,5),1)", 45299));
    CHECK(table->run("=NOW()").number() > 45000);
    CHECK(table->run("=TODAY()").number() > 45000);
}

void test_financial() {
    CHECK(eq("=PMT(0.08/12,10,10000)", -1037.03208935915, 1e-9));
    CHECK(eq("=PV(0.08/12,240,500,,0)", -59777.1458511879, 1e-9));
    CHECK(eq("=FV(0.06/12,10,-200,-500,1)", 2581.40337406012, 1e-9));
    CHECK(eq("=NPER(0.12/12,-100,-1000,10000,1)", 59.6738656742946, 1e-9));
    CHECK(eq("=RATE(4*12,-200,8000)*12", 0.0924, 1e-3));
    CHECK(eq("=IPMT(0.1/12,1,36,8000)", -66.6666666666667, 1e-9));
    CHECK(eq("=IPMT(0.1,3,3,8000)", -292.447129909366, 1e-9));
    CHECK(eq("=PPMT(0.1/12,1,24,2000)", -75.6231860083, 1e-8));
    CHECK(eq("=NPV(0.1,-10000,3000,4200,6800)", 1188.44341233522, 1e-9));
    CHECK(eq("=IRR({-70000,12000,15000,18000,21000,26000})", 0.0866309, 1e-5));
    CHECK(eq("=MIRR({-120000,39000,30000,21000,37000,46000},0.1,0.12)",
             0.126094, 1e-5));
    CHECK(eq("=XNPV(0.09,{-10000,2750,4250,3250,2750},{39448,39508,39751,39859,39904})",
             2086.65, 1e-5));
    CHECK(eq("=XIRR({-10000,2750,4250,3250,2750},{39448,39508,39751,39859,39904})",
             0.373362535, 1e-5));
    CHECK(eq("=SLN(30000,7500,10)", 2250));
    CHECK(eq("=SYD(30000,7500,10,1)", 4090.90909090909, 1e-9));
    CHECK(eq("=DDB(2400,300,10,1)", 480));
    CHECK(eq("=DDB(2400,300,10,2)", 384));
    CHECK(eq("=DB(1000000,100000,6,1,7)", 186083.333333, 1e-8));
    CHECK(eq("=DB(1000000,100000,6,2,7)", 259639.416667, 1e-8));
    CHECK(eq("=EFFECT(0.0525,4)", 0.0535426673, 1e-8));
    CHECK(eq("=NOMINAL(0.053543,4)", 0.05250032, 1e-6));
    CHECK(eq("=ISPMT(0.1/12,1,36,8000000)", -64814.8148148, 1e-8));
    CHECK(eq("=CUMIPMT(0.09/12,360,125000,13,24,0)", -11135.23213, 1e-7));
    CHECK(eq("=CUMIPMT(0.09/12,360,125000,1,1,0)", -937.5, 1e-9));
    CHECK(eq("=CUMPRINC(0.09/12,360,125000,13,24,0)", -934.1071234, 1e-7));
    CHECK(eq("=DOLLARDE(1.02,16)", 1.125));
    CHECK(eq("=DOLLARFR(1.125,16)", 1.02));
}

void test_info() {
    CHECK(is("=ISBLANK(Z99)", "TRUE"));
    CHECK(is("=ISBLANK(A1)", "FALSE"));
    CHECK(is("=ISNUMBER(B2)", "TRUE"));
    CHECK(is("=ISNUMBER(A1)", "FALSE"));
    CHECK(is("=ISTEXT(A1)", "TRUE"));
    CHECK(is("=ISNONTEXT(B2)", "TRUE"));
    CHECK(is("=ISLOGICAL(TRUE)", "TRUE"));
    CHECK(is("=ISERROR(1/0)", "TRUE"));
    CHECK(is("=ISERR(NA())", "FALSE"));
    CHECK(is("=ISERR(1/0)", "TRUE"));
    CHECK(is("=ISNA(NA())", "TRUE"));
    CHECK(is("=ISEVEN(4)", "TRUE"));
    CHECK(is("=ISODD(4)", "FALSE"));
    CHECK(is("=ISREF(A1)", "TRUE"));
    CHECK(is("=ISREF(1)", "FALSE"));
    CHECK(eq("=TYPE(1)", 1));
    CHECK(eq("=TYPE(\"a\")", 2));
    CHECK(eq("=TYPE(TRUE)", 4));
    CHECK(eq("=TYPE(1/0)", 16));
    CHECK(eq("=TYPE(A1:B2)", 64));
    CHECK(eq("=ERROR.TYPE(1/0)", 2));
    CHECK(eq("=N(TRUE)", 1));
    CHECK(eq("=N(\"a\")", 0));
    CHECK(fails("=NA()", error_code::na));
    CHECK(fails("=ERROR(\"#REF!\")", error_code::ref));
}

void test_database() {
    CHECK(eq("=DSUM(A1:C5,\"Qty\",F1:F2)", 12));
    CHECK(eq("=DAVERAGE(A1:C5,\"Qty\",F1:F2)", 6));
    CHECK(eq("=DMAX(A1:C5,\"Qty\",F1:F2)", 10));
    CHECK(eq("=DMIN(A1:C5,2,F1:F2)", 2));
    CHECK(eq("=DCOUNT(A1:C5,\"Qty\",F1:F2)", 2));
    CHECK(eq("=DCOUNTA(A1:C5,\"Name\",F1:F2)", 2));
    CHECK(eq("=DPRODUCT(A1:C5,\"Qty\",F1:F2)", 20));
    CHECK(fails("=DGET(A1:C5,\"Qty\",F1:F2)", error_code::num));
    CHECK(eq("=DVAR(A1:C5,\"Qty\",F1:F2)", 32));
    CHECK(eq("=DSTDEVP(A1:C5,\"Qty\",F1:F2)", 4));
}

void test_eng() {
    CHECK(eq("=BIN2DEC(1100100)", 100));
    CHECK(eq("=BIN2DEC(1111111111)", -1));
    CHECK(is("=BIN2OCT(1001,3)", "011"));
    CHECK(is("=BIN2HEX(11111011,4)", "00FB"));
    CHECK(eq("=OCT2DEC(144)", 100));
    CHECK(is("=OCT2BIN(3,3)", "011"));
    CHECK(is("=OCT2HEX(100,4)", "0040"));
    CHECK(eq("=HEX2DEC(\"FF\")", 255));
    CHECK(eq("=HEX2DEC(\"FFFFFFFFFF\")", -1));
    CHECK(is("=HEX2BIN(\"F\",8)", "00001111"));
    CHECK(is("=HEX2OCT(\"F\",3)", "017"));
    CHECK(is("=DEC2BIN(9,4)", "1001"));
    CHECK(is("=DEC2BIN(-1)", "1111111111"));
    CHECK(is("=DEC2OCT(58,3)", "072"));
    CHECK(is("=DEC2HEX(100,4)", "0064"));
    CHECK(fails("=DEC2BIN(9,1)", error_code::num));
    CHECK(eq("=BITAND(5,3)", 1));
    CHECK(eq("=BITOR(5,3)", 7));
    CHECK(eq("=BITXOR(5,3)", 6));
    CHECK(eq("=BITLSHIFT(1,4)", 16));
    CHECK(eq("=BITRSHIFT(16,4)", 1));
    CHECK(eq("=DELTA(1,1)", 1));
    CHECK(eq("=GESTEP(5,4)", 1));
    CHECK(eq("=ERF(1)", 0.842700792949715));
    CHECK(eq("=ERFC(1)", 0.157299207050285));
}

void test_matrix() {
    CHECK(eq("=MDETERM({1,2;3,4})", -2));
    CHECK(eq("=MDETERM({2,0,0;0,3,0;0,0,4})", 24));
    CHECK(eq("=INDEX(MINVERSE({4,-1;2,0}),1,2)", 0.5));
    CHECK(eq("=INDEX(MINVERSE({4,-1;2,0}),2,1)", -1));
    CHECK(eq("=INDEX(MMULT({1,2;3,4},{1;1}),2,1)", 7));
    CHECK(eq("=SUM(MUNIT(3))", 3));
}

void test_complex() {
    CHECK(is("=COMPLEX(3,2)", "3+2i"));
    CHECK(is("=COMPLEX(3,-1)", "3-i"));
    CHECK(is("=COMPLEX(0,1)", "i"));
    CHECK(is("=COMPLEX(3,2,\"j\")", "3+2j"));
    CHECK(eq("=IMREAL(\"3+2i\")", 3));
    CHECK(eq("=IMAGINARY(\"3+2i\")", 2));
    CHECK(eq("=IMAGINARY(\"-i\")", -1));
    CHECK(eq("=IMABS(\"3+4i\")", 5));
    CHECK(is("=IMCONJUGATE(\"3+2i\")", "3-2i"));
    CHECK(is("=IMSUM(\"1+i\",\"2+3i\")", "3+4i"));
    CHECK(is("=IMSUB(\"5+3i\",\"2+i\")", "3+2i"));
    CHECK(is("=IMPRODUCT(\"1+i\",\"1-i\")", "2"));
    CHECK(is("=IMDIV(\"2\",\"1+i\")", "1-i"));
    CHECK(eq("=IMABS(IMSQRT(\"-4\"))", 2));
    CHECK(eq("=IMREAL(IMEXP(\"0\"))", 1));
    CHECK(eq("=IMREAL(IMPOWER(\"2\",3))", 8));
    CHECK(fails("=IMABS(\"nonsense\")", error_code::num));
    CHECK(fails("=IMDIV(\"1\",\"0\")", error_code::num));
}

void test_numtheory() {
    CHECK(eq("=ITHPRIME(5)", 11));
    CHECK(eq("=NT_PI(100)", 25));
    CHECK(eq("=NT_PHI(36)", 12));
    CHECK(eq("=NT_D(36)", 9));
    CHECK(eq("=NT_SIGMA(36)", 91));
    CHECK(eq("=NT_MU(6)", 1));
    CHECK(eq("=NT_MU(4)", 0));
    CHECK(eq("=NT_OMEGA(30)", 3));
    CHECK(is("=ISPRIME(97)", "TRUE"));
    CHECK(eq("=PFACTOR(91)", 7));
    CHECK(eq("=SINPI(0.5)", 1));
    CHECK(eq("=POCHHAMMER(3,2)", 12));
}

void test_catalog() {
    const auto all = func::builtin_functions().all();
    CHECK(all.size() >= 300);
    for (const expr::function_def *item : all) {
        CHECK(!item->summary.empty());
        CHECK(!item->category.empty());
    }
}

} // namespace

int main() {
    bench shared;
    table = &shared;
    test_math();
    test_stat();
    test_string();
    test_logical();
    test_lookup();
    test_date();
    test_financial();
    test_info();
    test_database();
    test_eng();
    test_matrix();
    test_complex();
    test_numtheory();
    test_catalog();
    return testing::summary("functions");
}
