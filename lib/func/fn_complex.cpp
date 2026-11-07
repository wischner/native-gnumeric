//
// Complex numbers, which spreadsheets carry as text such as "3+2i".
//
// Derived from Gnumeric plugins/fn-complex/functions.c,
// Copyright (C) 1998-2002 Jukka-Pekka Iivonen, Morten Welinder.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include <cmath>
#include <complex>
#include <cstdio>
#include <cstdlib>

#include "func/collect.h"
#include "util/text.h"

namespace func {
namespace {

using number = std::complex<double>;

// Read one signed real at a position; returns where it ended.
const char *read_real(const char *text, double &out) {
    char *end = nullptr;
    out = std::strtod(text, &end);
    return end;
}

// Parse "a", "bi", "a+bi", "i" and "-j".
std::optional<number> parse_complex(const std::string &source) {
    const std::string text(util::trim(source));
    if (text.empty())
        return std::nullopt;
    const char *start = text.c_str();
    const char last = text.back();
    const bool imaginary = last == 'i' || last == 'j';
    if (!imaginary) {
        double real = 0;
        const char *end = read_real(start, real);
        if (*end != '\0')
            return std::nullopt;
        return number(real, 0);
    }
    // The imaginary part starts at the last sign that is not an exponent's.
    std::size_t split = text.size() - 1;
    while (split > 0) {
        const char c = text[split];
        if ((c == '+' || c == '-') && text[split - 1] != 'e' &&
            text[split - 1] != 'E')
            break;
        --split;
    }
    double real = 0;
    if (split > 0) {
        const char *end = read_real(start, real);
        if (end != start + split)
            return std::nullopt;
    }
    const std::string coefficient =
        text.substr(split, text.size() - 1 - split);
    double imag = 1;
    if (coefficient == "-") {
        imag = -1;
    } else if (coefficient != "+" && !coefficient.empty()) {
        const char *end = read_real(coefficient.c_str(), imag);
        if (*end != '\0')
            return std::nullopt;
    }
    return number(real, imag);
}

// Format one component with fifteen significant digits.
std::string part_text(double value) {
    char buffer[64];
    std::snprintf(buffer, sizeof buffer, "%.15g", value);
    return buffer;
}

// Format a complex number as a spreadsheet writes it.
value format_complex(number z, char unit = 'i') {
    if (!std::isfinite(z.real()) || !std::isfinite(z.imag()))
        return value(error_code::num);
    if (z.imag() == 0)
        return value(part_text(z.real()));
    std::string out;
    if (z.real() != 0)
        out = part_text(z.real());
    const double imag = z.imag();
    if (imag > 0 && !out.empty())
        out += '+';
    if (imag == -1)
        out += '-';
    else if (imag != 1)
        out += part_text(imag);
    out += unit;
    return value(out);
}

// Read an argument; numbers count as real.
std::optional<number> to_complex(const value &item) {
    if (item.is_string())
        return parse_complex(item.string());
    const auto real = item.to_number();
    if (!real)
        return std::nullopt;
    return number(*real, 0);
}

// Wrap a function from complex to complex.
template <number (*f)(number)> value of_complex(call_context &, args_t a) {
    const auto z = to_complex(a[0]);
    return z ? format_complex(f(*z)) : value(error_code::num);
}

number sin_of(number z) { return std::sin(z); }
number cos_of(number z) { return std::cos(z); }
number tan_of(number z) { return std::tan(z); }
number sec_of(number z) { return 1.0 / std::cos(z); }
number csc_of(number z) { return 1.0 / std::sin(z); }
number cot_of(number z) { return 1.0 / std::tan(z); }
number sinh_of(number z) { return std::sinh(z); }
number cosh_of(number z) { return std::cosh(z); }
number tanh_of(number z) { return std::tanh(z); }
number sech_of(number z) { return 1.0 / std::cosh(z); }
number csch_of(number z) { return 1.0 / std::sinh(z); }
number coth_of(number z) { return 1.0 / std::tanh(z); }
number asin_of(number z) { return std::asin(z); }
number acos_of(number z) { return std::acos(z); }
number atan_of(number z) { return std::atan(z); }
number asinh_of(number z) { return std::asinh(z); }
number acosh_of(number z) { return std::acosh(z); }
number atanh_of(number z) { return std::atanh(z); }
number acot_of(number z) { return std::atan(1.0 / z); }
number asec_of(number z) { return std::acos(1.0 / z); }
number acsc_of(number z) { return std::asin(1.0 / z); }
number acoth_of(number z) { return std::atanh(1.0 / z); }
number asech_of(number z) { return std::acosh(1.0 / z); }
number acsch_of(number z) { return std::asinh(1.0 / z); }
number exp_of(number z) { return std::exp(z); }
number ln_of(number z) { return std::log(z); }
number log10_of(number z) { return std::log10(z); }
number log2_of(number z) { return std::log(z) / std::log(2.0); }
number sqrt_of(number z) { return std::sqrt(z); }
number conjugate(number z) { return std::conj(z); }
number negate(number z) { return -z; }

// Wrap a function from complex to real.
template <double (*f)(number)> value part_of(call_context &, args_t a) {
    const auto z = to_complex(a[0]);
    return z ? checked(f(*z)) : value(error_code::num);
}

double real_part(number z) { return z.real(); }
double imag_part(number z) { return z.imag(); }
double modulus(number z) { return std::abs(z); }
double argument(number z) { return std::arg(z); }

value build(call_context &, args_t a) {
    const std::string suffix =
        a.size() > 2 && !a[2].is_empty() ? a[2].string() : "i";
    if (suffix != "i" && suffix != "j")
        return value(error_code::value);
    return format_complex({a[0].number(), a[1].number()}, suffix[0]);
}

value power(call_context &, args_t a) {
    const auto z = to_complex(a[0]);
    const auto w = to_complex(a[1]);
    if (!z || !w)
        return value(error_code::num);
    return format_complex(std::pow(*z, *w));
}

value divide(call_context &, args_t a) {
    const auto z = to_complex(a[0]);
    const auto w = to_complex(a[1]);
    if (!z || !w || *w == number(0, 0))
        return value(error_code::num);
    return format_complex(*z / *w);
}

value subtract(call_context &, args_t a) {
    const auto z = to_complex(a[0]);
    const auto w = to_complex(a[1]);
    return z && w ? format_complex(*z - *w) : value(error_code::num);
}

// Fold every argument with a complex operation.
template <number (*step)(number, number)>
value fold(call_context &context, nodes_t nodes, number start) {
    number total = start;
    const auto failed = for_each_value(
        context, nodes, [&](const value &item) -> std::optional<value> {
            if (item.is_error())
                return item;
            if (item.is_empty())
                return std::nullopt;
            const auto z = to_complex(item);
            if (!z)
                return value(error_code::num);
            total = step(total, *z);
            return std::nullopt;
        });
    return failed ? *failed : format_complex(total);
}

number add_step(number a, number b) { return a + b; }
number multiply_step(number a, number b) { return a * b; }

const function_row rows[] = {
    {"COMPLEX", "ff|s", build, "a complex number from its parts"},
    {"IMREAL", "S", part_of<real_part>, "real part"},
    {"IMAGINARY", "S", part_of<imag_part>, "imaginary part"},
    {"IMABS", "S", part_of<modulus>, "modulus"},
    {"IMARGUMENT", "S", part_of<argument>, "argument"},
    {"IMCONJUGATE", "S", of_complex<conjugate>, "conjugate"},
    {"IMNEG", "S", of_complex<negate>, "negation"},
    {"IMSUB", "SS", subtract, "difference"},
    {"IMDIV", "SS", divide, "quotient"},
    {"IMPOWER", "SS", power, "power"},
    {"IMEXP", "S", of_complex<exp_of>, "exponential"},
    {"IMLN", "S", of_complex<ln_of>, "natural logarithm"},
    {"IMLOG10", "S", of_complex<log10_of>, "base-10 logarithm"},
    {"IMLOG2", "S", of_complex<log2_of>, "base-2 logarithm"},
    {"IMSQRT", "S", of_complex<sqrt_of>, "square root"},
    {"IMSIN", "S", of_complex<sin_of>, "sine"},
    {"IMCOS", "S", of_complex<cos_of>, "cosine"},
    {"IMTAN", "S", of_complex<tan_of>, "tangent"},
    {"IMSEC", "S", of_complex<sec_of>, "secant"},
    {"IMCSC", "S", of_complex<csc_of>, "cosecant"},
    {"IMCOT", "S", of_complex<cot_of>, "cotangent"},
    {"IMSINH", "S", of_complex<sinh_of>, "hyperbolic sine"},
    {"IMCOSH", "S", of_complex<cosh_of>, "hyperbolic cosine"},
    {"IMTANH", "S", of_complex<tanh_of>, "hyperbolic tangent"},
    {"IMSECH", "S", of_complex<sech_of>, "hyperbolic secant"},
    {"IMCSCH", "S", of_complex<csch_of>, "hyperbolic cosecant"},
    {"IMCOTH", "S", of_complex<coth_of>, "hyperbolic cotangent"},
    {"IMARCSIN", "S", of_complex<asin_of>, "arc sine"},
    {"IMARCCOS", "S", of_complex<acos_of>, "arc cosine"},
    {"IMARCTAN", "S", of_complex<atan_of>, "arc tangent"},
    {"IMARCCOT", "S", of_complex<acot_of>, "arc cotangent"},
    {"IMARCSEC", "S", of_complex<asec_of>, "arc secant"},
    {"IMARCCSC", "S", of_complex<acsc_of>, "arc cosecant"},
    {"IMARCSINH", "S", of_complex<asinh_of>, "inverse hyperbolic sine"},
    {"IMARCCOSH", "S", of_complex<acosh_of>, "inverse hyperbolic cosine"},
    {"IMARCTANH", "S", of_complex<atanh_of>, "inverse hyperbolic tangent"},
    {"IMARCCOTH", "S", of_complex<acoth_of>, "inverse hyperbolic cotangent"},
    {"IMARCSECH", "S", of_complex<asech_of>, "inverse hyperbolic secant"},
    {"IMARCCSCH", "S", of_complex<acsch_of>, "inverse hyperbolic cosecant"},
};

const variadic_row variadic[] = {
    {"IMSUM",
     [](call_context &context, nodes_t nodes) -> value {
         return fold<add_step>(context, nodes, number(0, 0));
     },
     "sum"},
    {"IMPRODUCT",
     [](call_context &context, nodes_t nodes) -> value {
         return fold<multiply_step>(context, nodes, number(1, 0));
     },
     "product"},
};

} // namespace

void register_complex(expr::function_table &table) {
    table.add_rows("Complex", rows);
    add_variadic(table, "Complex", variadic);
}

} // namespace func
