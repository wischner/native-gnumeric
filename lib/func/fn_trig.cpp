//
// Trigonometric, hyperbolic, exponential and logarithmic functions.
//
// Derived from Gnumeric plugins/fn-math/functions.c,
// Copyright (C) 1998-2004 Jukka-Pekka Iivonen, Morten Welinder and others.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include <cmath>
#include <numbers>

#include "func/collect.h"
#include "func/mathfunc.h"

namespace func {
namespace {

// Wrap a function of one number.
template <double (*f)(double)> value of_one(call_context &, args_t a) {
    return checked(f(a[0].number()));
}

double sec(double x) { return 1 / std::cos(x); }
double csc(double x) { return 1 / std::sin(x); }
double cot(double x) { return 1 / std::tan(x); }
double sech(double x) { return 1 / std::cosh(x); }
double csch(double x) { return 1 / std::sinh(x); }
double coth(double x) { return 1 / std::tanh(x); }
double acot(double x) { return (std::numbers::pi / 2) - std::atan(x); }
double acoth(double x) { return std::atanh(1 / x); }
double asec(double x) { return std::acos(1 / x); }
double acsc(double x) { return std::asin(1 / x); }
double asech(double x) { return std::acosh(1 / x); }
double acsch(double x) { return std::asinh(1 / x); }
double degrees(double x) { return x * 180 / std::numbers::pi; }
double radians(double x) { return x * std::numbers::pi / 180; }
double sin_of(double x) { return std::sin(x); }
double cos_of(double x) { return std::cos(x); }
double tan_of(double x) { return std::tan(x); }
double asin_of(double x) { return std::asin(x); }
double acos_of(double x) { return std::acos(x); }
double atan_of(double x) { return std::atan(x); }
double sinh_of(double x) { return std::sinh(x); }
double cosh_of(double x) { return std::cosh(x); }
double tanh_of(double x) { return std::tanh(x); }
double asinh_of(double x) { return std::asinh(x); }
double acosh_of(double x) { return std::acosh(x); }
double atanh_of(double x) { return std::atanh(x); }
double exp_of(double x) { return std::exp(x); }
double expm1_of(double x) { return std::expm1(x); }
double sqrt_of(double x) { return std::sqrt(x); }
double sqrtpi(double x) { return std::sqrt(x * std::numbers::pi); }
double gamma_of(double x) {
    if (x <= 0 && x == std::floor(x))
        return NAN;
    return std::tgamma(x);
}
double gd(double x) { return std::atan(std::tanh(x / 2)) * 2; }

// Natural logarithm, an error at zero and below.
double ln_of(double x) { return x > 0 ? std::log(x) : NAN; }
double log2_of(double x) { return x > 0 ? std::log2(x) : NAN; }
double log10_of(double x) { return x > 0 ? std::log10(x) : NAN; }
double ln1p_of(double x) { return x > -1 ? std::log1p(x) : NAN; }
double gammaln(double x) {
    return x > 0 ? std::lgamma(x) : NAN;
}

const function_row rows[] = {
    {"SIN", "f", of_one<sin_of>, "sine"},
    {"COS", "f", of_one<cos_of>, "cosine"},
    {"TAN", "f", of_one<tan_of>, "tangent"},
    {"COT", "f", of_one<cot>, "cotangent"},
    {"SEC", "f", of_one<sec>, "secant"},
    {"CSC", "f", of_one<csc>, "cosecant"},
    {"ASIN", "f", of_one<asin_of>, "arc sine"},
    {"ACOS", "f", of_one<acos_of>, "arc cosine"},
    {"ATAN", "f", of_one<atan_of>, "arc tangent"},
    {"ACOT", "f", of_one<acot>, "arc cotangent"},
    {"ASEC", "f", of_one<asec>, "arc secant"},
    {"ACSC", "f", of_one<acsc>, "arc cosecant"},
    {"SINH", "f", of_one<sinh_of>, "hyperbolic sine"},
    {"COSH", "f", of_one<cosh_of>, "hyperbolic cosine"},
    {"TANH", "f", of_one<tanh_of>, "hyperbolic tangent"},
    {"COTH", "f", of_one<coth>, "hyperbolic cotangent"},
    {"SECH", "f", of_one<sech>, "hyperbolic secant"},
    {"CSCH", "f", of_one<csch>, "hyperbolic cosecant"},
    {"ASINH", "f", of_one<asinh_of>, "inverse hyperbolic sine"},
    {"ACOSH", "f", of_one<acosh_of>, "inverse hyperbolic cosine"},
    {"ATANH", "f", of_one<atanh_of>, "inverse hyperbolic tangent"},
    {"ACOTH", "f", of_one<acoth>, "inverse hyperbolic cotangent"},
    {"ASECH", "f", of_one<asech>, "inverse hyperbolic secant"},
    {"ACSCH", "f", of_one<acsch>, "inverse hyperbolic cosecant"},
    {"DEGREES", "f", of_one<degrees>, "radians to degrees"},
    {"RADIANS", "f", of_one<radians>, "degrees to radians"},
    {"EXP", "f", of_one<exp_of>, "e raised to a power"},
    {"EXPM1", "f", of_one<expm1_of>, "exp(x)-1"},
    {"LN", "f", of_one<ln_of>, "natural logarithm"},
    {"LN1P", "f", of_one<ln1p_of>, "ln(1+x)"},
    {"LOG2", "f", of_one<log2_of>, "base-2 logarithm"},
    {"LOG10", "f", of_one<log10_of>, "base-10 logarithm"},
    {"SQRT", "f", of_one<sqrt_of>, "square root"},
    {"SQRTPI", "f", of_one<sqrtpi>, "square root of x times pi"},
    {"GAMMA", "f", of_one<gamma_of>, "the gamma function"},
    {"GAMMALN", "f", of_one<gammaln>, "log of the gamma function"},
    {"GD", "f", of_one<gd>, "Gudermannian function"},
    {"LOG", "f|f",
     [](call_context &, args_t a) -> value {
         const double x = a[0].number();
         const double base = number_or(a, 1, 10);
         if (x <= 0 || base <= 0 || base == 1)
             return value(error_code::num);
         return checked(std::log(x) / std::log(base));
     },
     "logarithm to a base"},
    {"ATAN2", "ff",
     [](call_context &, args_t a) -> value {
         const double x = a[0].number();
         const double y = a[1].number();
         if (x == 0 && y == 0)
             return value(error_code::div0);
         return checked(std::atan2(y, x));
     },
     "arc tangent of y/x"},
    {"PI", "",
     [](call_context &, args_t) -> value { return value(std::numbers::pi); },
     "the constant pi"},
    {"HYPOT", "ff",
     [](call_context &, args_t a) -> value {
         return checked(std::hypot(a[0].number(), a[1].number()));
     },
     "the hypotenuse"},
    {"BETA", "ff",
     [](call_context &, args_t a) -> value {
         return checked(std::exp(log_beta(a[0].number(), a[1].number())));
     },
     "the beta function"},
    {"BETALN", "ff",
     [](call_context &, args_t a) -> value {
         return checked(log_beta(a[0].number(), a[1].number()));
     },
     "log of the beta function"},
};

} // namespace

void register_trig(expr::function_table &table) {
    table.add_rows("Mathematics", rows);
}

} // namespace func
