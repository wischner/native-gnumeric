//
// Probability distributions as spreadsheet functions.
//
// Derived from Gnumeric plugins/fn-stat/functions.c,
// Copyright (C) 1998-2002 Jukka-Pekka Iivonen, Morten Welinder.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include <cmath>

#include "func/collect.h"
#include "func/mathfunc.h"

namespace func {
namespace {

value norm_dist(call_context &, args_t a) {
    const double x = a[0].number();
    const double mean = a[1].number();
    const double sd = a[2].number();
    if (sd <= 0)
        return value(error_code::num);
    const double z = (x - mean) / sd;
    return checked(flag_or(a, 3, true) ? normal_cdf(z) : normal_pdf(z) / sd);
}

value norm_inv(call_context &, args_t a) {
    const double p = a[0].number();
    if (p <= 0 || p >= 1 || a[2].number() <= 0)
        return value(error_code::num);
    return checked(normal_quantile(p) * a[2].number() + a[1].number());
}

value lognorm_dist(call_context &, args_t a) {
    const double x = a[0].number();
    if (x <= 0 || a[2].number() <= 0)
        return value(error_code::num);
    return checked(
        normal_cdf((std::log(x) - a[1].number()) / a[2].number()));
}

value lognorm_inv(call_context &, args_t a) {
    const double p = a[0].number();
    if (p <= 0 || p >= 1 || a[2].number() <= 0)
        return value(error_code::num);
    return checked(
        std::exp(normal_quantile(p) * a[2].number() + a[1].number()));
}

// Excel-style upper-tail t with one or two tails.
value t_dist(call_context &, args_t a) {
    const double x = a[0].number();
    const double df = std::floor(a[1].number());
    const double tails = std::floor(a[2].number());
    if (x < 0 || df < 1 || (tails != 1 && tails != 2))
        return value(error_code::num);
    return checked(tails * (1 - student_t_cdf(x, df)));
}

value t_inv(call_context &, args_t a) {
    const double p = a[0].number();
    const double df = std::floor(a[1].number());
    if (p <= 0 || p > 1 || df < 1)
        return value(error_code::num);
    if (p == 1)
        return value(0.0);
    const auto cdf = [df](double t) { return student_t_cdf(t, df); };
    return checked(invert_cdf(cdf, 1 - p / 2, 0, HUGE_VAL));
}

value chi_dist(call_context &, args_t a) {
    const double x = a[0].number();
    const double df = std::floor(a[1].number());
    if (x < 0 || df < 1)
        return value(error_code::num);
    return checked(1 - chi_squared_cdf(x, df));
}

value chi_inv(call_context &, args_t a) {
    const double p = a[0].number();
    const double df = std::floor(a[1].number());
    if (p <= 0 || p > 1 || df < 1)
        return value(error_code::num);
    const auto cdf = [df](double x) { return chi_squared_cdf(x, df); };
    return checked(invert_cdf(cdf, 1 - p, 0, HUGE_VAL));
}

value f_dist(call_context &, args_t a) {
    const double x = a[0].number();
    const double d1 = std::floor(a[1].number());
    const double d2 = std::floor(a[2].number());
    if (x < 0 || d1 < 1 || d2 < 1)
        return value(error_code::num);
    return checked(1 - f_cdf(x, d1, d2));
}

value f_inv(call_context &, args_t a) {
    const double p = a[0].number();
    const double d1 = std::floor(a[1].number());
    const double d2 = std::floor(a[2].number());
    if (p <= 0 || p > 1 || d1 < 1 || d2 < 1)
        return value(error_code::num);
    const auto cdf = [=](double x) { return f_cdf(x, d1, d2); };
    return checked(invert_cdf(cdf, 1 - p, 0, HUGE_VAL));
}

value binom_dist(call_context &, args_t a) {
    const double k = std::floor(a[0].number());
    const double n = std::floor(a[1].number());
    const double p = a[2].number();
    if (k < 0 || n < 0 || k > n || p < 0 || p > 1)
        return value(error_code::num);
    return checked(a[3].flag() ? binomial_cdf(k, n, p)
                               : binomial_pmf(k, n, p));
}

value crit_binom(call_context &, args_t a) {
    const double n = std::floor(a[0].number());
    const double p = a[1].number();
    const double alpha = a[2].number();
    if (n < 0 || p < 0 || p > 1 || alpha < 0 || alpha > 1)
        return value(error_code::num);
    double total = 0;
    for (double k = 0; k <= n; ++k) {
        total += binomial_pmf(k, n, p);
        if (total >= alpha)
            return value(k);
    }
    return value(n);
}

value neg_binom(call_context &, args_t a) {
    const double f = std::floor(a[0].number());
    const double s = std::floor(a[1].number());
    const double p = a[2].number();
    if (f < 0 || s < 1 || p < 0 || p > 1)
        return value(error_code::num);
    return checked(binomial_coefficient(f + s - 1, s - 1) * std::pow(p, s) *
                   std::pow(1 - p, f));
}

value hyp_geom(call_context &, args_t a) {
    const double x = std::floor(a[0].number());
    const double n = std::floor(a[1].number());
    const double m = std::floor(a[2].number());
    const double total = std::floor(a[3].number());
    if (x < 0 || n < 0 || m < 0 || total <= 0 || n > total || m > total ||
        x > n || x > m || n - x > total - m)
        return value(error_code::num);
    const double mass = std::exp(
        std::lgamma(m + 1) - std::lgamma(x + 1) -
        std::lgamma(m - x + 1) + std::lgamma(total - m + 1) -
        std::lgamma(n - x + 1) - std::lgamma(total - m - n + x + 1) -
        (std::lgamma(total + 1) - std::lgamma(n + 1) -
         std::lgamma(total - n + 1)));
    return checked(mass);
}

value poisson(call_context &, args_t a) {
    const double k = std::floor(a[0].number());
    const double mean = a[1].number();
    if (k < 0 || mean < 0)
        return value(error_code::num);
    return checked(a[2].flag() ? poisson_cdf(k, mean)
                               : poisson_pmf(k, mean));
}

value expon(call_context &, args_t a) {
    const double x = a[0].number();
    const double lambda = a[1].number();
    if (x < 0 || lambda <= 0)
        return value(error_code::num);
    return checked(a[2].flag() ? -std::expm1(-lambda * x)
                               : lambda * std::exp(-lambda * x));
}

value gamma_dist(call_context &, args_t a) {
    const double x = a[0].number();
    const double shape = a[1].number();
    const double scale = a[2].number();
    if (x < 0 || shape <= 0 || scale <= 0)
        return value(error_code::num);
    if (a[3].flag())
        return checked(gamma_cdf(x, shape, scale));
    return checked(std::exp((shape - 1) * std::log(x) - x / scale -
                            log_gamma(shape) - shape * std::log(scale)));
}

value gamma_inv(call_context &, args_t a) {
    const double p = a[0].number();
    const double shape = a[1].number();
    const double scale = a[2].number();
    if (p < 0 || p >= 1 || shape <= 0 || scale <= 0)
        return value(error_code::num);
    if (p == 0)
        return value(0.0);
    const auto cdf = [=](double x) { return gamma_cdf(x, shape, scale); };
    return checked(invert_cdf(cdf, p, 0, HUGE_VAL));
}

value beta_dist(call_context &, args_t a) {
    const double low = number_or(a, 3, 0);
    const double high = number_or(a, 4, 1);
    const double x = a[0].number();
    if (x < low || x > high || a[1].number() <= 0 || a[2].number() <= 0 ||
        low == high)
        return value(error_code::num);
    return checked(beta_cdf((x - low) / (high - low), a[1].number(),
                            a[2].number()));
}

value beta_inv(call_context &, args_t a) {
    const double low = number_or(a, 3, 0);
    const double high = number_or(a, 4, 1);
    const double p = a[0].number();
    const double alpha = a[1].number();
    const double beta = a[2].number();
    if (p < 0 || p > 1 || alpha <= 0 || beta <= 0 || low == high)
        return value(error_code::num);
    const auto cdf = [=](double x) { return beta_cdf(x, alpha, beta); };
    return checked(low + invert_cdf(cdf, p, 0, 1) * (high - low));
}

value weibull(call_context &, args_t a) {
    const double x = a[0].number();
    const double shape = a[1].number();
    const double scale = a[2].number();
    if (x < 0 || shape <= 0 || scale <= 0)
        return value(error_code::num);
    const double ratio = std::pow(x / scale, shape);
    if (a[3].flag())
        return checked(-std::expm1(-ratio));
    return checked(shape / scale * std::pow(x / scale, shape - 1) *
                   std::exp(-ratio));
}

value fisher(call_context &, args_t a) {
    const double x = a[0].number();
    if (x <= -1 || x >= 1)
        return value(error_code::num);
    return checked(std::atanh(x));
}

value confidence(call_context &, args_t a) {
    const double alpha = a[0].number();
    const double sd = a[1].number();
    const double n = std::floor(a[2].number());
    if (alpha <= 0 || alpha >= 1 || sd <= 0 || n < 1)
        return value(error_code::num);
    return checked(-normal_quantile(alpha / 2) * sd / std::sqrt(n));
}

value standardize(call_context &, args_t a) {
    if (a[2].number() <= 0)
        return value(error_code::num);
    return checked((a[0].number() - a[1].number()) / a[2].number());
}

value permut(call_context &, args_t a) {
    const double n = std::floor(a[0].number());
    const double k = std::floor(a[1].number());
    if (n < 0 || k < 0 || k > n)
        return value(error_code::num);
    return checked(std::round(
        std::exp(log_gamma(n + 1) - log_gamma(n - k + 1))));
}

const function_row rows[] = {
    {"NORMDIST", "fff|b", norm_dist, "normal distribution"},
    {"NORMINV", "fff", norm_inv, "normal quantile"},
    {"NORMSDIST", "f",
     [](call_context &, args_t a) -> value {
         return checked(normal_cdf(a[0].number()));
     },
     "standard normal distribution"},
    {"NORMSINV", "f",
     [](call_context &, args_t a) -> value {
         const double p = a[0].number();
         if (p <= 0 || p >= 1)
             return value(error_code::num);
         return checked(normal_quantile(p));
     },
     "standard normal quantile"},
    {"LOGNORMDIST", "fff", lognorm_dist, "lognormal distribution"},
    {"LOGINV", "fff", lognorm_inv, "lognormal quantile"},
    {"TDIST", "fff", t_dist, "Student's t tail"},
    {"TINV", "ff", t_inv, "two-tailed Student's t quantile"},
    {"CHIDIST", "ff", chi_dist, "chi-squared tail"},
    {"CHIINV", "ff", chi_inv, "chi-squared quantile"},
    {"FDIST", "fff", f_dist, "F tail"},
    {"FINV", "fff", f_inv, "F quantile"},
    {"BINOMDIST", "fffb", binom_dist, "binomial distribution"},
    {"CRITBINOM", "fff", crit_binom, "smallest count reaching a probability"},
    {"NEGBINOMDIST", "fff", neg_binom, "negative binomial mass"},
    {"HYPGEOMDIST", "ffff", hyp_geom, "hypergeometric mass"},
    {"POISSON", "ffb", poisson, "Poisson distribution"},
    {"EXPONDIST", "ffb", expon, "exponential distribution"},
    {"GAMMADIST", "fffb", gamma_dist, "gamma distribution"},
    {"GAMMAINV", "fff", gamma_inv, "gamma quantile"},
    {"BETADIST", "fff|ff", beta_dist, "beta distribution"},
    {"BETAINV", "fff|ff", beta_inv, "beta quantile"},
    {"WEIBULL", "fffb", weibull, "Weibull distribution"},
    {"FISHER", "f", fisher, "Fisher transformation"},
    {"FISHERINV", "f",
     [](call_context &, args_t a) -> value {
         return checked(std::tanh(a[0].number()));
     },
     "inverse Fisher transformation"},
    {"CONFIDENCE", "fff", confidence, "half-width of a confidence interval"},
    {"STANDARDIZE", "fff", standardize, "normalized value"},
    {"PERMUT", "ff", permut, "permutations"},
};

} // namespace

void register_stat_dist(expr::function_table &table) {
    table.add_rows("Statistics", rows);
}

} // namespace func
