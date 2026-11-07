//
// Special functions, from the published algorithms named in the header.
//
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include "func/mathfunc.h"

#include <cmath>
#include <numbers>
#include <limits>

namespace func {
namespace {

constexpr int max_terms = 1000;
constexpr double tiny = 1e-300;
constexpr double epsilon = 1e-16;
constexpr double not_a_number = std::numeric_limits<double>::quiet_NaN();
constexpr double infinity = std::numeric_limits<double>::infinity();

// Series for P(a, x), good below a + 1.
double gamma_series(double a, double x) {
    double term = 1 / a;
    double sum = term;
    double denominator = a;
    for (int i = 0; i < max_terms; ++i) {
        denominator += 1;
        term *= x / denominator;
        sum += term;
        if (std::fabs(term) < std::fabs(sum) * epsilon)
            break;
    }
    return sum * std::exp(-x + a * std::log(x) - log_gamma(a));
}

// Continued fraction for Q(a, x), good above a + 1.
double gamma_fraction(double a, double x) {
    double b = x + 1 - a;
    double c = 1 / tiny;
    double d = 1 / b;
    double h = d;
    for (int i = 1; i < max_terms; ++i) {
        const double an = -i * (i - a);
        b += 2;
        d = an * d + b;
        if (std::fabs(d) < tiny)
            d = tiny;
        c = b + an / c;
        if (std::fabs(c) < tiny)
            c = tiny;
        d = 1 / d;
        const double delta = d * c;
        h *= delta;
        if (std::fabs(delta - 1) < epsilon)
            break;
    }
    return std::exp(-x + a * std::log(x) - log_gamma(a)) * h;
}

// Continued fraction for the incomplete beta.
double beta_fraction(double x, double a, double b) {
    const double qab = a + b;
    const double qap = a + 1;
    const double qam = a - 1;
    double c = 1;
    double d = 1 - qab * x / qap;
    if (std::fabs(d) < tiny)
        d = tiny;
    d = 1 / d;
    double h = d;
    for (int m = 1; m < max_terms; ++m) {
        const int m2 = 2 * m;
        double aa = m * (b - m) * x / ((qam + m2) * (a + m2));
        d = 1 + aa * d;
        if (std::fabs(d) < tiny)
            d = tiny;
        c = 1 + aa / c;
        if (std::fabs(c) < tiny)
            c = tiny;
        d = 1 / d;
        h *= d * c;
        aa = -(a + m) * (qab + m) * x / ((a + m2) * (qap + m2));
        d = 1 + aa * d;
        if (std::fabs(d) < tiny)
            d = tiny;
        c = 1 + aa / c;
        if (std::fabs(c) < tiny)
            c = tiny;
        d = 1 / d;
        const double delta = d * c;
        h *= delta;
        if (std::fabs(delta - 1) < epsilon)
            break;
    }
    return h;
}

// Evaluate a polynomial from the highest coefficient.
template <std::size_t size>
double horner(const double (&coefficients)[size], double x) {
    double result = 0;
    for (const double coefficient : coefficients)
        result = result * x + coefficient;
    return result;
}

} // namespace

double log_gamma(double x) {
    return std::lgamma(x);
}

double log_beta(double a, double b) {
    return log_gamma(a) + log_gamma(b) - log_gamma(a + b);
}

double binomial_coefficient(double n, double k) {
    if (n < 0 || k < 0 || k > n)
        return not_a_number;
    if (n < 171)
        return std::round(std::exp(log_gamma(n + 1) - log_gamma(k + 1) -
                                   log_gamma(n - k + 1)));
    return std::exp(log_gamma(n + 1) - log_gamma(k + 1) -
                    log_gamma(n - k + 1));
}

double regularized_gamma(double a, double x) {
    if (x < 0 || a <= 0)
        return not_a_number;
    if (x == 0)
        return 0;
    if (std::isinf(x))
        return 1;
    return x < a + 1 ? gamma_series(a, x) : 1 - gamma_fraction(a, x);
}

double regularized_beta(double x, double a, double b) {
    if (x < 0 || x > 1 || a <= 0 || b <= 0)
        return not_a_number;
    if (x == 0 || x == 1)
        return x;
    const double front = std::exp(a * std::log(x) + b * std::log1p(-x) -
                                  log_beta(a, b));
    if (x < (a + 1) / (a + b + 2))
        return front * beta_fraction(x, a, b) / a;
    return 1 - front * beta_fraction(1 - x, b, a) / b;
}

double normal_cdf(double z) {
    return 0.5 * std::erfc(-z * (1 / std::numbers::sqrt2));
}

double normal_pdf(double z) {
    return std::exp(-0.5 * z * z) / std::sqrt(2 * std::numbers::pi);
}

double normal_quantile(double p) {
    if (p < 0 || p > 1 || std::isnan(p))
        return not_a_number;
    if (p == 0)
        return -infinity;
    if (p == 1)
        return infinity;
    // Algorithm AS 241 (PPND16), Wichura 1988.
    static constexpr double a[] = {
        2.5090809287301226727e3, 3.3430575583588128105e4,
        6.7265770927008700853e4, 4.5921953931549871457e4,
        1.3731693765509461125e4, 1.9715909503065514427e3,
        1.3314166789178437745e2, 3.3871328727963666080e0};
    static constexpr double b[] = {
        5.2264952788528545610e3, 2.8729085735721942674e4,
        3.9307895800092710610e4, 2.1213794301586595867e4,
        5.3941960214247511077e3, 6.8718700749205790830e2,
        4.2313330701600911252e1, 1.0};
    static constexpr double c[] = {
        7.74545014278341407640e-4, 2.27238449892691845833e-2,
        2.41780725177450611770e-1, 1.27045825245236838258e0,
        3.64784832476320460504e0,  5.76949722146069140550e0,
        4.63033784615654529590e0,  1.42343711074968357734e0};
    static constexpr double d[] = {
        1.05075007164441684324e-9, 5.47593808499534494600e-4,
        1.51986665636164571966e-2, 1.48103976427480074590e-1,
        6.89767334985100004550e-1, 1.67638483018380384940e0,
        2.05319162663775882187e0,  1.0};
    static constexpr double e[] = {
        2.01033439929228813265e-7, 2.71155556874348757815e-5,
        1.24266094738807843860e-3, 2.65321895265761230930e-2,
        2.96560571828504891230e-1, 1.78482653991729133580e0,
        5.46378491116411436990e0,  6.65790464350110377720e0};
    static constexpr double f[] = {
        2.04426310338993978564e-15, 1.42151175831644588870e-7,
        1.84631831751005468180e-5,  7.86869131145613259100e-4,
        1.48753612908506148525e-2,  1.36929880922735805310e-1,
        5.99832206555887937690e-1,  1.0};
    const double q = p - 0.5;
    if (std::fabs(q) <= 0.425) {
        const double r = 0.180625 - q * q;
        return q * horner(a, r) / horner(b, r);
    }
    double r = q < 0 ? p : 1 - p;
    r = std::sqrt(-std::log(r));
    double result = 0;
    if (r <= 5) {
        r -= 1.6;
        result = horner(c, r) / horner(d, r);
    } else {
        r -= 5;
        result = horner(e, r) / horner(f, r);
    }
    return q < 0 ? -result : result;
}

double student_t_cdf(double t, double df) {
    const double x = df / (df + t * t);
    const double tail = 0.5 * regularized_beta(x, df / 2, 0.5);
    return t > 0 ? 1 - tail : tail;
}

double chi_squared_cdf(double x, double df) {
    return x <= 0 ? 0 : regularized_gamma(df / 2, x / 2);
}

double f_cdf(double x, double df1, double df2) {
    if (x <= 0)
        return 0;
    return regularized_beta(df1 * x / (df1 * x + df2), df1 / 2, df2 / 2);
}

double gamma_cdf(double x, double shape, double scale) {
    return x <= 0 ? 0 : regularized_gamma(shape, x / scale);
}

double beta_cdf(double x, double a, double b) {
    if (x <= 0)
        return 0;
    if (x >= 1)
        return 1;
    return regularized_beta(x, a, b);
}

double binomial_pmf(double k, double n, double p) {
    if (k < 0 || k > n)
        return 0;
    if (p == 0)
        return k == 0 ? 1 : 0;
    if (p == 1)
        return k == n ? 1 : 0;
    return std::exp(log_gamma(n + 1) - log_gamma(k + 1) -
                    log_gamma(n - k + 1) + k * std::log(p) +
                    (n - k) * std::log1p(-p));
}

double binomial_cdf(double k, double n, double p) {
    if (k < 0)
        return 0;
    if (k >= n)
        return 1;
    if (p == 0)
        return 1;
    if (p == 1)
        return 0;
    return regularized_beta(1 - p, n - k, k + 1);
}

double poisson_pmf(double k, double lambda) {
    // The definition itself, in logarithms for range.
    if (k < 0)
        return 0;
    if (lambda == 0)
        return k == 0 ? 1 : 0;
    return std::exp(k * std::log(lambda) - lambda - log_gamma(k + 1));
}

double poisson_cdf(double k, double lambda) {
    if (k < 0)
        return 0;
    return lambda == 0 ? 1 : 1 - regularized_gamma(k + 1, lambda);
}

double invert_cdf(const std::function<double(double)> &cdf, double p,
                  double low, double high) {
    if (std::isinf(low)) {
        low = -1;
        while (cdf(low) > p && low > -1e300)
            low *= 2;
    }
    if (std::isinf(high)) {
        high = low > 0 ? low * 2 : 1;
        while (cdf(high) < p && high < 1e300)
            high *= 2;
    }
    for (int i = 0; i < 400; ++i) {
        const double middle = 0.5 * (low + high);
        if (middle == low || middle == high)
            break;
        if (cdf(middle) < p)
            low = middle;
        else
            high = middle;
    }
    return 0.5 * (low + high);
}

} // namespace func
