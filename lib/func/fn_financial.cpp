//
// Time value of money, cash flows and depreciation.
//
// Derived from Gnumeric plugins/fn-financial/functions.c,
// Copyright (C) 1998-2002 Vladimir Vuksan, Jukka-Pekka Iivonen,
// Morten Welinder.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include <algorithm>
#include <cmath>

#include "func/collect.h"
#include "numfmt/date.h"

namespace func {
namespace {

using expr::auto_format;

// Return (1+rate)^n accurately for tiny rates.
double growth(double rate, double periods) {
    return std::exp(periods * std::log1p(rate));
}

double fv_of(double rate, double nper, double pmt, double pv, int type) {
    if (rate == 0)
        return -(pv + pmt * nper);
    const double g = growth(rate, nper);
    return -(pv * g + pmt * (1 + rate * type) * (g - 1) / rate);
}

double pv_of(double rate, double nper, double pmt, double fv, int type) {
    if (rate == 0)
        return -(fv + pmt * nper);
    const double g = growth(rate, nper);
    return -(fv + pmt * (1 + rate * type) * (g - 1) / rate) / g;
}

double pmt_of(double rate, double nper, double pv, double fv, int type) {
    if (rate == 0)
        return -(pv + fv) / nper;
    const double g = growth(rate, nper);
    return -(fv + pv * g) * rate / ((1 + rate * type) * (g - 1));
}

value pv_fn(call_context &, args_t a) {
    return checked(pv_of(a[0].number(), a[1].number(), a[2].number(),
                         number_or(a, 3, 0),
                         static_cast<int>(number_or(a, 4, 0))));
}

value fv_fn(call_context &, args_t a) {
    return checked(fv_of(a[0].number(), a[1].number(), a[2].number(),
                         number_or(a, 3, 0),
                         static_cast<int>(number_or(a, 4, 0))));
}

value pmt_fn(call_context &, args_t a) {
    if (a[1].number() == 0)
        return value(error_code::num);
    return checked(pmt_of(a[0].number(), a[1].number(), a[2].number(),
                          number_or(a, 3, 0),
                          static_cast<int>(number_or(a, 4, 0))));
}

value nper_fn(call_context &, args_t a) {
    const double rate = a[0].number();
    const double pmt = a[1].number();
    const double pv = a[2].number();
    const double fv = number_or(a, 3, 0);
    const int type = static_cast<int>(number_or(a, 4, 0));
    if (rate == 0) {
        if (pmt == 0)
            return value(error_code::div0);
        return checked(-(pv + fv) / pmt);
    }
    const double adjusted = pmt * (1 + rate * type) / rate;
    const double ratio = (adjusted - fv) / (adjusted + pv);
    if (ratio <= 0)
        return value(error_code::num);
    return checked(std::log(ratio) / std::log1p(rate));
}

// Return the interest of one period.
double ipmt_of(double rate, double period, double nper, double pv, double fv,
               int type) {
    const double pmt = pmt_of(rate, nper, pv, fv, type);
    if (period == 1)
        return type ? 0 : -pv * rate;
    return type ? fv_of(rate, period - 2, pmt, pv, 1) * rate
                : fv_of(rate, period - 1, pmt, pv, 0) * rate;
}

value ipmt_fn(call_context &, args_t a) {
    const double period = a[1].number();
    const double nper = a[2].number();
    if (period < 1 || period > nper)
        return value(error_code::num);
    return checked(ipmt_of(a[0].number(), period, nper, a[3].number(),
                           number_or(a, 4, 0),
                           static_cast<int>(number_or(a, 5, 0))));
}

value ppmt_fn(call_context &, args_t a) {
    const double rate = a[0].number();
    const double period = a[1].number();
    const double nper = a[2].number();
    const double pv = a[3].number();
    const double fv = number_or(a, 4, 0);
    const int type = static_cast<int>(number_or(a, 5, 0));
    if (period < 1 || period > nper)
        return value(error_code::num);
    return checked(pmt_of(rate, nper, pv, fv, type) -
                   ipmt_of(rate, period, nper, pv, fv, type));
}

// Solve f(x)=0 by Newton's method with a numeric derivative, starting
// from a guess; falls back to scanning for a bracket.
template <typename f_t>
std::optional<double> solve(f_t f, double guess, double low_limit) {
    double x = guess;
    for (int i = 0; i < 100; ++i) {
        const double y = f(x);
        if (std::fabs(y) < 1e-10)
            return x;
        const double h = std::max(std::fabs(x), 1.0) * 1e-7;
        const double slope = (f(x + h) - y) / h;
        if (slope == 0 || !std::isfinite(slope))
            break;
        double next = x - y / slope;
        if (next <= low_limit)
            next = (x + low_limit) / 2;
        if (std::fabs(next - x) < 1e-13 * std::max(std::fabs(x), 1.0))
            return next;
        x = next;
    }
    double previous = low_limit + 1e-6;
    double previous_y = f(previous);
    double candidate = -0.99;
    while (candidate < 100) {
        candidate += 0.01 * std::max(1.0, std::fabs(candidate));
        const double y = f(candidate);
        if (std::isfinite(y) && std::isfinite(previous_y) &&
            (y < 0) != (previous_y < 0)) {
            double low = previous;
            double high = candidate;
            double low_y = previous_y;
            for (int i = 0; i < 200; ++i) {
                const double middle = (low + high) / 2;
                const double middle_y = f(middle);
                if ((middle_y < 0) == (low_y < 0)) {
                    low = middle;
                    low_y = middle_y;
                } else {
                    high = middle;
                }
            }
            return (low + high) / 2;
        }
        previous = candidate;
        previous_y = y;
    }
    return std::nullopt;
}

value rate_fn(call_context &, args_t a) {
    const double nper = a[0].number();
    const double pmt = a[1].number();
    const double pv = a[2].number();
    const double fv = number_or(a, 3, 0);
    const int type = static_cast<int>(number_or(a, 4, 0));
    const double guess = number_or(a, 5, 0.1);
    if (nper <= 0)
        return value(error_code::num);
    const auto f = [=](double rate) {
        return fv_of(rate, nper, pmt, pv, type) - fv;
    };
    const auto found = solve(f, guess, -1.0);
    return found ? checked(*found) : value(error_code::num);
}

value npv_fn(call_context &context, nodes_t nodes) {
    if (nodes.size() < 2)
        return value(error_code::value);
    const value rate_value =
        nodes[0]->eval(context.pos, expr::eval_mode::scalar);
    if (rate_value.is_error())
        return rate_value;
    const auto rate = rate_value.to_number();
    if (!rate)
        return value(error_code::value);
    std::vector<double> flows;
    if (auto failure =
            collect_numbers(context, nodes.subspan(1), {}, flows))
        return *failure;
    double total = 0;
    double factor = 1 + *rate;
    for (const double flow : flows) {
        total += flow / factor;
        factor *= 1 + *rate;
    }
    return checked(total);
}

value irr_fn(call_context &, args_t a) {
    const std::vector<double> flows = grid_numbers(a[0]);
    if (flows.size() < 2)
        return value(error_code::num);
    const auto f = [&](double rate) {
        double total = 0;
        double factor = 1;
        for (const double flow : flows) {
            total += flow / factor;
            factor *= 1 + rate;
        }
        return total;
    };
    const auto found = solve(f, number_or(a, 1, 0.1), -1.0);
    return found ? checked(*found) : value(error_code::num);
}

value mirr_fn(call_context &, args_t a) {
    const std::vector<double> flows = grid_numbers(a[0]);
    const double finance = a[1].number();
    const double reinvest = a[2].number();
    const double n = static_cast<double>(flows.size());
    if (n < 2)
        return value(error_code::div0);
    double positive = 0;
    double negative = 0;
    for (std::size_t i = 0; i < flows.size(); ++i) {
        if (flows[i] > 0)
            positive += flows[i] * growth(reinvest, n - 1 - i);
        else
            negative += flows[i] / growth(finance, static_cast<double>(i));
    }
    if (positive == 0 || negative == 0)
        return value(error_code::div0);
    return checked(std::pow(-positive / negative, 1 / (n - 1)) - 1);
}

// Return the XNPV of dated flows.
double xnpv_of(double rate, const std::vector<double> &flows,
               const std::vector<double> &dates) {
    double total = 0;
    for (std::size_t i = 0; i < flows.size(); ++i)
        total += flows[i] / growth(rate, (dates[i] - dates[0]) / 365);
    return total;
}

value xnpv_fn(call_context &, args_t a) {
    std::vector<double> flows;
    std::vector<double> dates;
    if (auto failure = paired_numbers(a[1], a[2], flows, dates))
        return *failure;
    if (flows.empty() || a[0].number() <= -1)
        return value(error_code::num);
    return checked(xnpv_of(a[0].number(), flows, dates));
}

value xirr_fn(call_context &, args_t a) {
    std::vector<double> flows;
    std::vector<double> dates;
    if (auto failure = paired_numbers(a[0], a[1], flows, dates))
        return *failure;
    if (flows.size() < 2)
        return value(error_code::num);
    const auto f = [&](double rate) { return xnpv_of(rate, flows, dates); };
    const auto found = solve(f, number_or(a, 2, 0.1), -1.0);
    return found ? checked(*found) : value(error_code::num);
}

value sln_fn(call_context &, args_t a) {
    if (a[2].number() == 0)
        return value(error_code::div0);
    return checked((a[0].number() - a[1].number()) / a[2].number());
}

value syd_fn(call_context &, args_t a) {
    const double cost = a[0].number();
    const double salvage = a[1].number();
    const double life = a[2].number();
    const double period = a[3].number();
    if (life <= 0 || period < 1 || period > life)
        return value(error_code::num);
    return checked((cost - salvage) * (life - period + 1) * 2 /
                   (life * (life + 1)));
}

value ddb_fn(call_context &, args_t a) {
    const double cost = a[0].number();
    const double salvage = a[1].number();
    const double life = a[2].number();
    const double period = a[3].number();
    const double factor = number_or(a, 4, 2);
    if (cost < 0 || salvage < 0 || life <= 0 || period <= 0 ||
        period > life || factor <= 0)
        return value(error_code::num);
    const double rate = std::min(factor / life, 1.0);
    double book = cost;
    double taken = 0;
    for (int i = 1; i <= std::ceil(period); ++i) {
        taken = std::max(0.0, std::min(book * rate, book - salvage));
        book -= taken;
    }
    return checked(taken);
}

value db_fn(call_context &, args_t a) {
    const double cost = a[0].number();
    const double salvage = a[1].number();
    const double life = a[2].number();
    const double period = a[3].number();
    const double month = number_or(a, 4, 12);
    if (cost <= 0 || salvage < 0 || life <= 0 || period < 1 || month < 1 ||
        month > 12)
        return value(error_code::num);
    const double rate =
        std::round((1 - std::pow(salvage / cost, 1 / life)) * 1000) / 1000;
    double book = cost;
    double taken = cost * rate * month / 12;
    if (period == 1)
        return checked(taken);
    book -= taken;
    for (int i = 2; i <= period; ++i) {
        taken = i == life + 1 ? book * rate * (12 - month) / 12 : book * rate;
        book -= taken;
    }
    return checked(taken);
}

value effect_fn(call_context &, args_t a) {
    const double rate = a[0].number();
    const double n = std::floor(a[1].number());
    if (rate <= 0 || n < 1)
        return value(error_code::num);
    return checked(std::expm1(n * std::log1p(rate / n)));
}

value nominal_fn(call_context &, args_t a) {
    const double rate = a[0].number();
    const double n = std::floor(a[1].number());
    if (rate <= 0 || n < 1)
        return value(error_code::num);
    return checked(n * std::expm1(std::log1p(rate) / n));
}

value ispmt_fn(call_context &, args_t a) {
    const double rate = a[0].number();
    const double period = a[1].number();
    const double nper = a[2].number();
    const double pv = a[3].number();
    if (nper == 0)
        return value(error_code::div0);
    return checked(pv * rate * (period / nper - 1));
}

// CUMIPMT and CUMPRINC share their argument check.
template <bool principal> value cumulative(call_context &, args_t a) {
    const double rate = a[0].number();
    const double nper = a[1].number();
    const double pv = a[2].number();
    const double first = a[3].number();
    const double last = a[4].number();
    const int type = static_cast<int>(a[5].number());
    if (rate <= 0 || nper <= 0 || pv <= 0 || first < 1 || last < first ||
        last > nper || (type != 0 && type != 1))
        return value(error_code::num);
    double total = 0;
    const double pmt = pmt_of(rate, nper, pv, 0, type);
    for (double period = first; period <= last; ++period) {
        const double interest = ipmt_of(rate, period, nper, pv, 0, type);
        total += principal ? pmt - interest : interest;
    }
    return checked(total);
}

value dollar_de(call_context &, args_t a) {
    const double price = a[0].number();
    const double fraction = std::floor(a[1].number());
    if (fraction <= 0)
        return value(error_code::num);
    const double digits = std::ceil(std::log10(fraction));
    const double whole = std::trunc(price);
    return checked(whole + (price - whole) * std::pow(10.0, digits) / fraction);
}

value dollar_fr(call_context &, args_t a) {
    const double price = a[0].number();
    const double fraction = std::floor(a[1].number());
    if (fraction <= 0)
        return value(error_code::num);
    const double digits = std::ceil(std::log10(fraction));
    const double whole = std::trunc(price);
    return checked(whole + (price - whole) * fraction / std::pow(10.0, digits));
}

value rri_fn(call_context &, args_t a) {
    const double nper = a[0].number();
    if (nper <= 0 || a[1].number() == 0)
        return value(error_code::num);
    return checked(std::pow(a[2].number() / a[1].number(), 1 / nper) - 1);
}

value pduration_fn(call_context &, args_t a) {
    const double rate = a[0].number();
    if (rate <= 0 || a[1].number() <= 0 || a[2].number() <= 0)
        return value(error_code::num);
    return checked((std::log(a[2].number()) - std::log(a[1].number())) /
                   std::log1p(rate));
}

const function_row rows[] = {
    {"PV", "fff|ff", pv_fn, "present value", auto_format::money},
    {"FV", "fff|ff", fv_fn, "future value", auto_format::money},
    {"PMT", "fff|ff", pmt_fn, "periodic payment", auto_format::money},
    {"IPMT", "ffff|ff", ipmt_fn, "interest part of a payment",
     auto_format::money},
    {"PPMT", "ffff|ff", ppmt_fn, "principal part of a payment",
     auto_format::money},
    {"NPER", "fff|ff", nper_fn, "number of periods"},
    {"RATE", "fff|fff", rate_fn, "periodic interest rate",
     auto_format::percent},
    {"IRR", "A|f", irr_fn, "internal rate of return", auto_format::percent},
    {"MIRR", "Aff", mirr_fn, "modified internal rate of return",
     auto_format::percent},
    {"XNPV", "fAA", xnpv_fn, "net present value of dated flows",
     auto_format::money},
    {"XIRR", "AA|f", xirr_fn, "internal rate of dated flows",
     auto_format::percent},
    {"SLN", "fff", sln_fn, "straight-line depreciation", auto_format::money},
    {"SYD", "ffff", syd_fn, "sum-of-years depreciation", auto_format::money},
    {"DDB", "ffff|f", ddb_fn, "double-declining depreciation",
     auto_format::money},
    {"DB", "ffff|f", db_fn, "fixed-declining depreciation",
     auto_format::money},
    {"EFFECT", "ff", effect_fn, "effective annual rate", auto_format::percent},
    {"NOMINAL", "ff", nominal_fn, "nominal annual rate", auto_format::percent},
    {"ISPMT", "ffff", ispmt_fn, "interest on a straight-line loan",
     auto_format::money},
    {"CUMIPMT", "ffffff", cumulative<false>, "cumulative interest paid",
     auto_format::money},
    {"CUMPRINC", "ffffff", cumulative<true>, "cumulative principal paid",
     auto_format::money},
    {"DOLLARDE", "ff", dollar_de, "fractional dollar to decimal"},
    {"DOLLARFR", "ff", dollar_fr, "decimal dollar to fraction"},
    {"RRI", "fff", rri_fn, "rate for an investment to grow",
     auto_format::percent},
    {"PDURATION", "fff", pduration_fn, "periods for an investment to grow"},
};

const variadic_row variadic[] = {
    {"NPV", npv_fn, "net present value", auto_format::money},
};

} // namespace

void register_financial(expr::function_table &table) {
    table.add_rows("Financial", rows);
    add_variadic(table, "Financial", variadic);
}

} // namespace func
