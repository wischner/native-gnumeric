//
// Correlation, regression and hypothesis tests.
//
// Derived from Gnumeric plugins/fn-stat/functions.c,
// Copyright (C) 1998-2002 Jukka-Pekka Iivonen, Morten Welinder.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include <algorithm>
#include <cmath>
#include <numeric>

#include "func/collect.h"
#include "func/mathfunc.h"

namespace func {
namespace {

// The sums a simple regression needs.
struct pair_sums {
    double n = 0;
    double mean_x = 0;
    double mean_y = 0;
    double sxx = 0;
    double syy = 0;
    double sxy = 0;
};

pair_sums sums_of(const std::vector<double> &xs,
                  const std::vector<double> &ys) {
    pair_sums s;
    s.n = static_cast<double>(xs.size());
    if (xs.empty())
        return s;
    s.mean_x = std::accumulate(xs.begin(), xs.end(), 0.0) / s.n;
    s.mean_y = std::accumulate(ys.begin(), ys.end(), 0.0) / s.n;
    for (std::size_t i = 0; i < xs.size(); ++i) {
        const double dx = xs[i] - s.mean_x;
        const double dy = ys[i] - s.mean_y;
        s.sxx += dx * dx;
        s.syy += dy * dy;
        s.sxy += dx * dy;
    }
    return s;
}

// Run a function of pair sums over two grids; y comes first or second.
template <value (*compute)(const pair_sums &), bool y_first>
value on_pairs(call_context &, args_t a) {
    std::vector<double> xs;
    std::vector<double> ys;
    if (auto failure = y_first ? paired_numbers(a[1], a[0], xs, ys)
                               : paired_numbers(a[0], a[1], xs, ys))
        return *failure;
    if (xs.empty())
        return value(error_code::div0);
    return compute(sums_of(xs, ys));
}

value correlation(const pair_sums &s) {
    if (s.sxx == 0 || s.syy == 0)
        return value(error_code::div0);
    return checked(s.sxy / std::sqrt(s.sxx * s.syy));
}
value r_squared(const pair_sums &s) {
    if (s.sxx == 0 || s.syy == 0)
        return value(error_code::div0);
    return checked(s.sxy * s.sxy / (s.sxx * s.syy));
}
value covariance(const pair_sums &s) {
    return checked(s.sxy / s.n);
}
value covariance_sample(const pair_sums &s) {
    if (s.n < 2)
        return value(error_code::div0);
    return checked(s.sxy / (s.n - 1));
}
value slope(const pair_sums &s) {
    if (s.sxx == 0)
        return value(error_code::div0);
    return checked(s.sxy / s.sxx);
}
value intercept(const pair_sums &s) {
    if (s.sxx == 0)
        return value(error_code::div0);
    return checked(s.mean_y - s.sxy / s.sxx * s.mean_x);
}
value standard_error(const pair_sums &s) {
    if (s.n < 3 || s.sxx == 0)
        return value(error_code::div0);
    return checked(
        std::sqrt((s.syy - s.sxy * s.sxy / s.sxx) / (s.n - 2)));
}

value forecast(call_context &, args_t a) {
    std::vector<double> xs;
    std::vector<double> ys;
    if (auto failure = paired_numbers(a[2], a[1], xs, ys))
        return *failure;
    const pair_sums s = sums_of(xs, ys);
    if (s.sxx == 0)
        return value(error_code::div0);
    const double b = s.sxy / s.sxx;
    return checked(s.mean_y + b * (a[0].number() - s.mean_x));
}

// Fit y = b + m x, optionally through the origin.
void fit_line(const std::vector<double> &xs, const std::vector<double> &ys,
              bool with_constant, double &m, double &b) {
    if (with_constant) {
        const pair_sums s = sums_of(xs, ys);
        m = s.sxx == 0 ? 0 : s.sxy / s.sxx;
        b = s.mean_y - m * s.mean_x;
        return;
    }
    double sxx = 0;
    double sxy = 0;
    for (std::size_t i = 0; i < xs.size(); ++i) {
        sxx += xs[i] * xs[i];
        sxy += xs[i] * ys[i];
    }
    m = sxx == 0 ? 0 : sxy / sxx;
    b = 0;
}

// Return 1, 2, 3... when known x are absent.
std::vector<double> default_xs(std::size_t count) {
    std::vector<double> xs(count);
    std::iota(xs.begin(), xs.end(), 1.0);
    return xs;
}

value linest(call_context &, args_t a) {
    std::vector<double> ys = grid_numbers(a[0]);
    std::vector<double> xs =
        a.size() > 1 && !a[1].is_empty() ? grid_numbers(a[1])
                                         : default_xs(ys.size());
    if (xs.size() != ys.size() || ys.empty())
        return value(error_code::na);
    double m = 0;
    double b = 0;
    fit_line(xs, ys, flag_or(a, 2, true), m, b);
    auto result = std::make_shared<val::array_value>(2, 1);
    result->set(0, 0, checked(m));
    result->set(1, 0, checked(b));
    return value(std::shared_ptr<const val::array_value>(std::move(result)));
}

value logest(call_context &, args_t a) {
    std::vector<double> ys = grid_numbers(a[0]);
    for (double &y : ys) {
        if (y <= 0)
            return value(error_code::num);
        y = std::log(y);
    }
    std::vector<double> xs =
        a.size() > 1 && !a[1].is_empty() ? grid_numbers(a[1])
                                         : default_xs(ys.size());
    if (xs.size() != ys.size() || ys.empty())
        return value(error_code::na);
    double m = 0;
    double b = 0;
    fit_line(xs, ys, flag_or(a, 2, true), m, b);
    auto result = std::make_shared<val::array_value>(2, 1);
    result->set(0, 0, checked(std::exp(m)));
    result->set(1, 0, checked(std::exp(b)));
    return value(std::shared_ptr<const val::array_value>(std::move(result)));
}

// TREND and GROWTH differ only by the logarithm around the fit.
template <bool exponential> value extrapolate(call_context &, args_t a) {
    std::vector<double> ys = grid_numbers(a[0]);
    if constexpr (exponential)
        for (double &y : ys) {
            if (y <= 0)
                return value(error_code::num);
            y = std::log(y);
        }
    std::vector<double> xs =
        a.size() > 1 && !a[1].is_empty() ? grid_numbers(a[1])
                                         : default_xs(ys.size());
    if (xs.size() != ys.size() || ys.empty())
        return value(error_code::na);
    double m = 0;
    double b = 0;
    fit_line(xs, ys, flag_or(a, 3, true), m, b);
    const bool has_new = a.size() > 2 && !a[2].is_empty();
    const std::vector<double> new_xs = has_new ? grid_numbers(a[2]) : xs;
    const int cols = has_new ? a[2].grid_cols() : a[0].grid_cols();
    const int rows = has_new ? a[2].grid_rows() : a[0].grid_rows();
    if (new_xs.size() != static_cast<std::size_t>(cols) * rows)
        return value(error_code::value);
    auto result = std::make_shared<val::array_value>(cols, rows);
    for (std::size_t i = 0; i < new_xs.size(); ++i) {
        const double y = m * new_xs[i] + b;
        double shown = y;
        if constexpr (exponential)
            shown = std::exp(y);
        result->set(static_cast<int>(i) % cols, static_cast<int>(i) / cols,
                    checked(shown));
    }
    return value(std::shared_ptr<const val::array_value>(std::move(result)));
}

// The mean and sample variance of a list.
struct summary {
    double n = 0;
    double mean = 0;
    double variance = 0;
};

summary summarize(const std::vector<double> &n) {
    summary s;
    s.n = static_cast<double>(n.size());
    if (n.empty())
        return s;
    s.mean = std::accumulate(n.begin(), n.end(), 0.0) / s.n;
    for (const double x : n)
        s.variance += (x - s.mean) * (x - s.mean);
    s.variance /= s.n > 1 ? s.n - 1 : 1;
    return s;
}

value t_test(call_context &, args_t a) {
    const double tails = std::floor(a[2].number());
    const double type = std::floor(a[3].number());
    if ((tails != 1 && tails != 2) || type < 1 || type > 3)
        return value(error_code::num);
    double t = 0;
    double df = 0;
    if (type == 1) {
        std::vector<double> xs;
        std::vector<double> ys;
        if (auto failure = paired_numbers(a[0], a[1], xs, ys))
            return *failure;
        std::vector<double> differences(xs.size());
        for (std::size_t i = 0; i < xs.size(); ++i)
            differences[i] = xs[i] - ys[i];
        const summary s = summarize(differences);
        if (s.n < 2 || s.variance == 0)
            return value(error_code::div0);
        t = s.mean / std::sqrt(s.variance / s.n);
        df = s.n - 1;
    } else {
        const summary x = summarize(grid_numbers(a[0]));
        const summary y = summarize(grid_numbers(a[1]));
        if (x.n < 2 || y.n < 2)
            return value(error_code::div0);
        if (type == 2) {
            df = x.n + y.n - 2;
            const double pooled = ((x.n - 1) * x.variance +
                                   (y.n - 1) * y.variance) /
                                  df;
            t = (x.mean - y.mean) / std::sqrt(pooled * (1 / x.n + 1 / y.n));
        } else {
            const double vx = x.variance / x.n;
            const double vy = y.variance / y.n;
            t = (x.mean - y.mean) / std::sqrt(vx + vy);
            df = (vx + vy) * (vx + vy) /
                 (vx * vx / (x.n - 1) + vy * vy / (y.n - 1));
        }
    }
    return checked(tails * (1 - student_t_cdf(std::fabs(t), df)));
}

value f_test(call_context &, args_t a) {
    const summary x = summarize(grid_numbers(a[0]));
    const summary y = summarize(grid_numbers(a[1]));
    if (x.n < 2 || y.n < 2 || x.variance == 0 || y.variance == 0)
        return value(error_code::div0);
    const double f = x.variance / y.variance;
    const double p = f_cdf(f, x.n - 1, y.n - 1);
    return checked(2 * std::min(p, 1 - p));
}

value z_test(call_context &, args_t a) {
    const summary s = summarize(grid_numbers(a[0]));
    const double mu = a[1].number();
    const double sigma =
        number_or(a, 2, std::sqrt(s.variance));
    if (s.n < 1 || sigma <= 0)
        return value(error_code::div0);
    return checked(1 - normal_cdf((s.mean - mu) / (sigma / std::sqrt(s.n))));
}

value chi_test(call_context &, args_t a) {
    value failure;
    const auto actual = to_matrix(a[0], failure);
    if (!actual)
        return failure;
    const auto expected = to_matrix(a[1], failure);
    if (!expected)
        return failure;
    if (actual->cols != expected->cols || actual->rows != expected->rows)
        return value(error_code::na);
    double statistic = 0;
    for (std::size_t i = 0; i < actual->cells.size(); ++i) {
        const double e = expected->cells[i];
        if (e == 0)
            return value(error_code::div0);
        statistic += (actual->cells[i] - e) * (actual->cells[i] - e) / e;
    }
    const int cols = actual->cols;
    const int rows = actual->rows;
    const double df = (cols > 1 && rows > 1) ? (cols - 1) * (rows - 1)
                                             : (cols * rows) - 1;
    return checked(1 - chi_squared_cdf(statistic, df));
}

value probability(call_context &, args_t a) {
    std::vector<double> xs;
    std::vector<double> ps;
    if (auto failure = paired_numbers(a[0], a[1], xs, ps))
        return *failure;
    const double low = a[2].number();
    const double high = number_or(a, 3, low);
    double total = 0;
    double weight = 0;
    for (std::size_t i = 0; i < xs.size(); ++i) {
        if (ps[i] < 0 || ps[i] > 1)
            return value(error_code::num);
        weight += ps[i];
        if (xs[i] >= low && xs[i] <= high)
            total += ps[i];
    }
    if (std::fabs(weight - 1) > 1e-9)
        return value(error_code::num);
    return checked(total);
}

const function_row rows[] = {
    {"CORREL", "AA", on_pairs<correlation, false>, "correlation"},
    {"PEARSON", "AA", on_pairs<correlation, false>, "Pearson correlation"},
    {"RSQ", "AA", on_pairs<r_squared, true>, "square of the correlation"},
    {"COVAR", "AA", on_pairs<covariance, false>, "population covariance"},
    {"COVARIANCE.P", "AA", on_pairs<covariance, false>,
     "population covariance"},
    {"COVARIANCE.S", "AA", on_pairs<covariance_sample, false>,
     "sample covariance"},
    {"SLOPE", "AA", on_pairs<slope, true>, "slope of a regression line"},
    {"INTERCEPT", "AA", on_pairs<intercept, true>,
     "intercept of a regression line"},
    {"STEYX", "AA", on_pairs<standard_error, true>,
     "standard error of a regression"},
    {"FORECAST", "fAA", forecast, "a value on the regression line"},
    {"LINEST", "A|Ab", linest, "slope and intercept of a fit"},
    {"LOGEST", "A|Ab", logest, "base and factor of an exponential fit"},
    {"TREND", "A|AAb", extrapolate<false>, "values on a regression line"},
    {"GROWTH", "A|AAb", extrapolate<true>,
     "values on an exponential curve"},
    {"TTEST", "AAff", t_test, "Student's t-test"},
    {"FTEST", "AA", f_test, "F-test of variances"},
    {"ZTEST", "Af|f", z_test, "one-tailed z-test"},
    {"CHITEST", "AA", chi_test, "chi-squared test of independence"},
    {"PROB", "AAf|f", probability, "probability of a range of values"},
};

} // namespace

void register_stat_fit(expr::function_table &table) {
    table.add_rows("Statistics", rows);
}

} // namespace func
