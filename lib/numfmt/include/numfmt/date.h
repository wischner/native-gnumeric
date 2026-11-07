//
// Calendar conversions behind every date value. A date is a day count
// from the 1900 epoch with time as the fraction, keeping the Lotus
// fiction that 1900 was a leap year: serial 60 is 1900-02-29. Files and
// formulas depend on it, so it is reproduced exactly.
//
// Derived from goffice go-glib-extras and Gnumeric src/gnm-datetime.c,
// Copyright (C) 2005-2009 Morten Welinder and contributors.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#ifndef NUMFMT_DATE_H
#define NUMFMT_DATE_H

#include <optional>

namespace numfmt {

// A calendar date.
struct civil_date {
    int year = 1900;
    int month = 1;
    int day = 1;

    friend bool operator==(const civil_date &, const civil_date &) = default;
};

// A time of day; seconds keep their fraction.
struct civil_time {
    int hour = 0;
    int minute = 0;
    double second = 0;
};

// Report whether a Gregorian year is a leap year.
bool is_leap_year(int year);

// Return the days in a month.
int days_in_month(int year, int month);

// Return the serial of a date, normalizing overflowing months and days.
double serial_from_date(int year, int month, int day);

// Return the date of a serial; nothing before day 0 or past 9999.
std::optional<civil_date> date_from_serial(double serial);

// Return the time of day in a serial's fraction.
civil_time time_from_serial(double serial);

// Return the weekday of a serial, Sunday being 0.
int weekday_from_serial(double serial);

} // namespace numfmt

#endif
