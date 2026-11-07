//
// Date and time functions.
//
// Derived from Gnumeric plugins/fn-date/functions.c,
// Copyright (C) 1998-2002 Jukka-Pekka Iivonen, Morten Welinder.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <set>

#include "func/collect.h"
#include "numfmt/date.h"
#include "numfmt/match.h"

namespace func {
namespace {

using expr::auto_format;
using numfmt::civil_date;
using numfmt::date_from_serial;
using numfmt::serial_from_date;

// Return the local time as a serial with a fraction.
double now_serial() {
    const std::time_t clock = std::time(nullptr);
    std::tm parts{};
#if defined(_WIN32)
    localtime_s(&parts, &clock);
#else
    localtime_r(&clock, &parts);
#endif
    return serial_from_date(parts.tm_year + 1900, parts.tm_mon + 1,
                            parts.tm_mday) +
           (parts.tm_hour * 3600.0 + parts.tm_min * 60.0 + parts.tm_sec) /
               86400.0;
}

// Read a date part from a serial.
template <int civil_date::*part> value date_part(call_context &, args_t a) {
    const auto date = date_from_serial(a[0].number());
    return date ? value((*date).*part) : value(error_code::num);
}

value date_of(call_context &, args_t a) {
    const double year = std::floor(a[0].number());
    const double month = std::floor(a[1].number());
    const double day = std::floor(a[2].number());
    if (year < 0 || year >= 10000)
        return value(error_code::num);
    const int full = year < 1900 ? static_cast<int>(year) + 1900
                                 : static_cast<int>(year);
    return checked(serial_from_date(full, static_cast<int>(month),
                                    static_cast<int>(day)));
}

value time_of(call_context &, args_t a) {
    const double seconds = a[0].number() * 3600 + a[1].number() * 60 +
                           a[2].number();
    if (seconds < 0)
        return value(error_code::num);
    return checked(std::fmod(seconds, 86400) / 86400);
}

value date_value(call_context &, args_t a) {
    const auto match = numfmt::match_number(a[0].string());
    return match ? value(std::floor(match->value))
                 : value(error_code::value);
}

value time_value(call_context &, args_t a) {
    const auto match = numfmt::match_number(a[0].string());
    return match ? value(match->value - std::floor(match->value))
                 : value(error_code::value);
}

// Return a serial's time of day in whole seconds, rounded.
int whole_seconds(double serial) {
    const double fraction = serial - std::floor(serial);
    return static_cast<int>(std::round(fraction * 86400)) % 86400;
}

value hour_of(call_context &, args_t a) {
    return value(whole_seconds(a[0].number()) / 3600);
}
value minute_of(call_context &, args_t a) {
    return value(whole_seconds(a[0].number()) / 60 % 60);
}
value second_of(call_context &, args_t a) {
    return value(whole_seconds(a[0].number()) % 60);
}

value weekday_of(call_context &, args_t a) {
    const int day = numfmt::weekday_from_serial(a[0].number());
    const int kind = static_cast<int>(number_or(a, 1, 1));
    switch (kind) {
    case 1:
        return value(day + 1);
    case 2:
        return value((day + 6) % 7 + 1);
    case 3:
        return value((day + 6) % 7);
    default:
        return value(error_code::num);
    }
}

// Return the ISO week of a date.
int iso_week(const civil_date &date, double serial) {
    const int monday_based = (numfmt::weekday_from_serial(serial) + 6) % 7;
    const double thursday = serial - monday_based + 3;
    const auto year = date_from_serial(thursday);
    const double first = serial_from_date(year ? year->year : date.year, 1, 1);
    return static_cast<int>((thursday - first) / 7) + 1;
}

value week_number(call_context &, args_t a) {
    const double serial = std::floor(a[0].number());
    const auto date = date_from_serial(serial);
    if (!date)
        return value(error_code::num);
    const int kind = static_cast<int>(number_or(a, 1, 1));
    if (kind == 150 || kind == 21)
        return value(iso_week(*date, serial));
    if (kind != 1 && kind != 2)
        return value(error_code::num);
    const double first = serial_from_date(date->year, 1, 1);
    const int first_day =
        (numfmt::weekday_from_serial(first) + (kind == 2 ? 6 : 0)) % 7;
    return value(static_cast<int>((serial - first + first_day) / 7) + 1);
}

value iso_week_number(call_context &, args_t a) {
    const double serial = std::floor(a[0].number());
    const auto date = date_from_serial(serial);
    return date ? value(iso_week(*date, serial)) : value(error_code::num);
}

// Add months, clamping the day to the month's end.
std::optional<double> shift_months(double serial, int months, bool to_end) {
    const auto date = date_from_serial(serial);
    if (!date)
        return std::nullopt;
    const int total = date->year * 12 + (date->month - 1) + months;
    const int year = total >= 0 ? total / 12 : (total - 11) / 12;
    const int month = total - year * 12 + 1;
    if (year < 1900 || year > 9999)
        return std::nullopt;
    const int last = numfmt::days_in_month(year, month);
    return serial_from_date(year, month,
                            to_end ? last : std::min(date->day, last));
}

value edate(call_context &, args_t a) {
    const auto shifted = shift_months(
        a[0].number(), static_cast<int>(std::trunc(a[1].number())), false);
    return shifted ? value(*shifted) : value(error_code::num);
}

value eomonth(call_context &, args_t a) {
    const auto shifted = shift_months(
        a[0].number(), static_cast<int>(std::trunc(number_or(a, 1, 0))), true);
    return shifted ? value(*shifted) : value(error_code::num);
}

// Return the 30/360 day count, US or European.
double days_360(const civil_date &a, const civil_date &b, bool european) {
    int d1 = a.day;
    int d2 = b.day;
    if (european) {
        d1 = std::min(d1, 30);
        d2 = std::min(d2, 30);
    } else {
        const bool last_feb =
            a.month == 2 && d1 == numfmt::days_in_month(a.year, 2);
        if (last_feb)
            d1 = 30;
        if (d2 == 31 && d1 >= 30)
            d2 = 30;
        d1 = std::min(d1, 30);
    }
    return (b.year - a.year) * 360.0 + (b.month - a.month) * 30.0 + d2 - d1;
}

value days360(call_context &, args_t a) {
    const auto first = date_from_serial(a[0].number());
    const auto second = date_from_serial(a[1].number());
    if (!first || !second)
        return value(error_code::num);
    return value(days_360(*first, *second, number_or(a, 2, 0) != 0));
}

value date_dif(call_context &, args_t a) {
    const double start = std::floor(a[0].number());
    const double end = std::floor(a[1].number());
    if (start > end)
        return value(error_code::num);
    const auto from = date_from_serial(start);
    const auto to = date_from_serial(end);
    if (!from || !to)
        return value(error_code::num);
    std::string unit = a[2].string();
    for (char &c : unit)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    int months = (to->year - from->year) * 12 + (to->month - from->month);
    if (to->day < from->day)
        --months;
    if (unit == "d")
        return value(end - start);
    if (unit == "m")
        return value(months);
    if (unit == "y")
        return value(months / 12);
    if (unit == "ym")
        return value(months % 12);
    if (unit == "yd") {
        const int years = months / 12;
        const auto base = shift_months(start, years * 12, false);
        return base ? value(end - *base) : value(error_code::num);
    }
    if (unit == "md") {
        const auto base = shift_months(start, months, false);
        return base ? value(end - *base) : value(error_code::num);
    }
    return value(error_code::num);
}

value year_frac(call_context &, args_t a) {
    double start = std::floor(a[0].number());
    double end = std::floor(a[1].number());
    const int basis = static_cast<int>(number_or(a, 2, 0));
    if (start > end)
        std::swap(start, end);
    const auto from = date_from_serial(start);
    const auto to = date_from_serial(end);
    if (!from || !to || basis < 0 || basis > 4)
        return value(error_code::num);
    switch (basis) {
    case 0:
        return value(days_360(*from, *to, false) / 360);
    case 1: {
        double total = 0;
        for (int year = from->year; year <= to->year; ++year)
            total += numfmt::is_leap_year(year) ? 366 : 365;
        const double years = to->year - from->year + 1;
        return value((end - start) / (total / years));
    }
    case 2:
        return value((end - start) / 360);
    case 3:
        return value((end - start) / 365);
    default:
        return value(days_360(*from, *to, true) / 360);
    }
}

// Read holidays from a grid into a set of serials.
std::set<double> holiday_set(args_t a, std::size_t index) {
    std::set<double> days;
    if (index < a.size())
        for (const double serial : grid_numbers(a[index]))
            days.insert(std::floor(serial));
    return days;
}

// Report whether a serial falls on a weekend.
bool is_weekend(double serial) {
    const int day = numfmt::weekday_from_serial(serial);
    return day == 0 || day == 6;
}

value network_days(call_context &, args_t a) {
    double start = std::floor(a[0].number());
    double end = std::floor(a[1].number());
    const double sign = start > end ? -1 : 1;
    if (start > end)
        std::swap(start, end);
    if (end - start > 4000000)
        return value(error_code::num);
    const std::set<double> holidays = holiday_set(a, 2);
    double count = 0;
    for (double day = start; day <= end; ++day)
        if (!is_weekend(day) && !holidays.count(day))
            ++count;
    return value(sign * count);
}

value work_day(call_context &, args_t a) {
    double day = std::floor(a[0].number());
    double remaining = std::trunc(a[1].number());
    const double step = remaining < 0 ? -1 : 1;
    const std::set<double> holidays = holiday_set(a, 2);
    remaining = std::fabs(remaining);
    if (remaining > 4000000)
        return value(error_code::num);
    while (remaining > 0) {
        day += step;
        if (day < 1)
            return value(error_code::num);
        if (!is_weekend(day) && !holidays.count(day))
            --remaining;
    }
    return value(day);
}

value days_between(call_context &, args_t a) {
    return value(std::floor(a[0].number()) - std::floor(a[1].number()));
}

const function_row rows[] = {
    {"DATE", "fff", date_of, "a serial from year, month and day",
     auto_format::date},
    {"TIME", "fff", time_of, "a fraction from hour, minute and second",
     auto_format::time},
    {"DATEVALUE", "s", date_value, "the date a text spells"},
    {"TIMEVALUE", "s", time_value, "the time a text spells"},
    {"NOW", "",
     [](call_context &, args_t) -> value { return value(now_serial()); },
     "the current date and time", auto_format::date},
    {"TODAY", "",
     [](call_context &, args_t) -> value {
         return value(std::floor(now_serial()));
     },
     "the current date", auto_format::date},
    {"YEAR", "f", date_part<&civil_date::year>, "year of a date"},
    {"MONTH", "f", date_part<&civil_date::month>, "month of a date"},
    {"DAY", "f", date_part<&civil_date::day>, "day of a date"},
    {"HOUR", "f", hour_of, "hour of a time"},
    {"MINUTE", "f", minute_of, "minute of a time"},
    {"SECOND", "f", second_of, "second of a time"},
    {"WEEKDAY", "f|f", weekday_of, "day of the week"},
    {"WEEKNUM", "f|f", week_number, "week of the year"},
    {"ISOWEEKNUM", "f", iso_week_number, "ISO week of the year"},
    {"EDATE", "ff", edate, "a date some months away", auto_format::date},
    {"EOMONTH", "f|f", eomonth, "the end of a month", auto_format::date},
    {"DAYS", "ff", days_between, "days between two dates"},
    {"DAYS360", "ff|f", days360, "days between dates on a 360-day year"},
    {"DATEDIF", "ffs", date_dif, "difference between dates in a unit"},
    {"YEARFRAC", "ff|f", year_frac, "fraction of a year between dates"},
    {"NETWORKDAYS", "ff|A", network_days, "working days between dates"},
    {"WORKDAY", "ff|A", work_day, "a date some working days away",
     auto_format::date},
};

} // namespace

void register_date(expr::function_table &table) {
    table.add_rows("Date and time", rows);
    for (const std::string_view name : {"NOW", "TODAY"}) {
        expr::function_def definition = *table.find(name);
        definition.is_volatile = true;
        table.add(definition);
    }
}

} // namespace func
