//
// Special functions and probability distributions. These implement the
// published algorithms: continued fractions for the incomplete gamma
// and beta (Lentz), Wichura's AS 241 for the normal quantile, and
// root-finding on the distribution function for the other quantiles.
// Gnumeric's mathfunc.c holds R's nmath code, part of which is GPL 3
// or later, so none of it is ported (see docs/notes/LICENSING.md).
//
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#ifndef FUNC_MATHFUNC_H
#define FUNC_MATHFUNC_H

#include <functional>

namespace func {

// Return the natural logarithm of the gamma function.
double log_gamma(double x);

// Return the natural logarithm of the beta function.
double log_beta(double a, double b);

// Return n choose k, or a NaN when out of range.
double binomial_coefficient(double n, double k);

// Return the regularized lower incomplete gamma P(a, x).
double regularized_gamma(double a, double x);

// Return the regularized incomplete beta I_x(a, b).
double regularized_beta(double x, double a, double b);

// Return the standard normal distribution function.
double normal_cdf(double z);

// Return the standard normal density.
double normal_pdf(double z);

// Return the standard normal quantile; infinite at 0 and 1.
double normal_quantile(double p);

// Return Student's t distribution function.
double student_t_cdf(double t, double df);

// Return the chi-squared distribution function.
double chi_squared_cdf(double x, double df);

// Return the F distribution function.
double f_cdf(double x, double df1, double df2);

// Return the gamma distribution function with a scale.
double gamma_cdf(double x, double shape, double scale);

// Return the beta distribution function on [0, 1].
double beta_cdf(double x, double a, double b);

// Return the binomial probability mass.
double binomial_pmf(double k, double n, double p);

// Return the binomial distribution function.
double binomial_cdf(double k, double n, double p);

// Return the Poisson probability mass.
double poisson_pmf(double k, double lambda);

// Return the Poisson distribution function.
double poisson_cdf(double k, double lambda);

//
// Return the x where an increasing distribution function reaches p.
//
// Parameters:
//      cdf     - The distribution function.
//      p       - Target probability, strictly inside (0, 1).
//      low     - Lower bound of the support.
//      high    - Upper bound; infinity expands the bracket as needed.
//
double invert_cdf(const std::function<double(double)> &cdf, double p,
                  double low, double high);

} // namespace func

#endif
