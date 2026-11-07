//
// Civil-from-days arithmetic on the proleptic Gregorian calendar, with
// the 1900 leap-year fiction applied at the epoch.
//
// Derived from goffice go-glib-extras and Gnumeric src/gnm-datetime.c,
// Copyright (C) 2005-2009 Morten Welinder and contributors.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include "numfmt/date.h"

#include <cmath>
#include <cstdint>

namespace numfmt {
namespace {

// Days from 1970-01-01 to a civil date.
std::int64_t days_from_civil(std::int64_t y, int m, int d) {
    y -= m <= 2;
    const std::int64_t era = (y >= 0 ? y : y - 399) / 400;
    const auto yoe = static_cast<unsigned>(y - era * 400);
    const unsigned doy = (153u * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + static_cast<std::int64_t>(doe) - 719468;
}

// Civil date of a day count from 1970-01-01.
civil_date civil_from_days(std::int64_t z) {
    z += 719468;
    const std::int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    const auto doe = static_cast<unsigned>(z - era * 146097);
    const unsigned yoe =
        (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const std::int64_t y = static_cast<std::int64_t>(yoe) + era * 400;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned mp = (5 * doy + 2) / 153;
    const int d = static_cast<int>(doy - (153 * mp + 2) / 5 + 1);
    const int m = static_cast<int>(mp < 10 ? mp + 3 : mp - 9);
    return {static_cast<int>(y + (m <= 2)), m, d};
}

// 1899-12-31 is serial 0 for dates through February 1900; afterwards
// the fictitious leap day moves the epoch back one day.
const std::int64_t epoch = days_from_civil(1899, 12, 31);

} // namespace

bool is_leap_year(int year) {
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

int days_in_month(int year, int month) {
    static constexpr int lengths[] = {31, 28, 31, 30, 31, 30,
                                      31, 31, 30, 31, 30, 31};
    if (month == 2 && is_leap_year(year))
        return 29;
    return lengths[month - 1];
}

double serial_from_date(int year, int month, int day) {
    const int zero_month = month - 1;
    const int shift = zero_month >= 0 ? zero_month / 12
                                      : -((11 - zero_month) / 12);
    const int y = year + shift;
    const int m = zero_month - shift * 12 + 1;
    const std::int64_t days = days_from_civil(y, m, 1) + (day - 1);
    const std::int64_t serial = days - epoch;
    // March 1900 onward counts the day that never happened.
    return static_cast<double>(serial >= 60 ? serial + 1 : serial);
}

std::optional<civil_date> date_from_serial(double serial) {
    if (!std::isfinite(serial) || serial < 0 || serial >= 2958466)
        return std::nullopt;
    const auto whole = static_cast<std::int64_t>(std::floor(serial));
    if (whole == 60)
        return civil_date{1900, 2, 29};
    return civil_from_days(epoch + (whole > 60 ? whole - 1 : whole));
}

civil_time time_from_serial(double serial) {
    const double fraction = serial - std::floor(serial);
    // Rounded to the microsecond so 0.1 of a day prints 2:24:00.
    const double total = std::round(fraction * 86400.0 * 1e6) / 1e6;
    const int whole = static_cast<int>(total);
    civil_time time;
    time.hour = whole / 3600;
    time.minute = (whole / 60) % 60;
    time.second = total - (time.hour * 3600 + time.minute * 60);
    return time;
}

int weekday_from_serial(double serial) {
    const auto whole = static_cast<std::int64_t>(std::floor(serial));
    return static_cast<int>(((whole - 1) % 7 + 7) % 7);
}

} // namespace numfmt
