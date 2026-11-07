//
// Engineering functions: number bases, bit operations and step tests.
//
// Derived from Gnumeric plugins/fn-eng/functions.c,
// Copyright (C) 1998-2002 Jukka-Pekka Iivonen, Morten Welinder.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "func/collect.h"
#include "util/text.h"

namespace func {
namespace {

// Read text in a base; 10-digit forms are twos complement.
std::optional<double> from_base(const value &source, int base) {
    const std::string text = util::to_upper(source.to_text());
    if (text.empty() || text.size() > 10)
        return std::nullopt;
    std::int64_t result = 0;
    for (const char c : text) {
        const int digit = c >= '0' && c <= '9'   ? c - '0'
                          : c >= 'A' && c <= 'Z' ? c - 'A' + 10
                                                 : 99;
        if (digit >= base)
            return std::nullopt;
        result = result * base + digit;
    }
    if (text.size() == 10 && base != 10) {
        const int bits = base == 2 ? 10 : base == 8 ? 30 : 40;
        if (result >= (std::int64_t{1} << (bits - 1)))
            result -= std::int64_t{1} << bits;
    }
    return static_cast<double>(result);
}

// Write a number in a base, padded to places.
value to_base(double number, int base, double places) {
    const double whole = std::trunc(number);
    const int bits = base == 2 ? 10 : base == 8 ? 30 : 40;
    const double limit = std::ldexp(1.0, bits - 1);
    if (whole < -limit || whole >= limit)
        return value(error_code::num);
    auto left = static_cast<std::int64_t>(whole);
    if (left < 0)
        left += std::int64_t{1} << bits;
    std::string out;
    do {
        const int digit = static_cast<int>(left % base);
        out.insert(out.begin(),
                   static_cast<char>(digit < 10 ? '0' + digit
                                                : 'A' + digit - 10));
        left /= base;
    } while (left > 0);
    if (places > 0) {
        if (static_cast<double>(out.size()) > places)
            return value(error_code::num);
        out.insert(0, static_cast<std::size_t>(places) - out.size(), '0');
    }
    return value(out);
}

template <int source_base, int target_base>
value convert_base(call_context &, args_t a) {
    const auto number = from_base(a[0], source_base);
    if (!number)
        return value(error_code::num);
    if (target_base == 10)
        return value(*number);
    return to_base(*number, target_base, number_or(a, 1, 0));
}

template <int target_base> value from_decimal(call_context &, args_t a) {
    return to_base(a[0].number(), target_base, number_or(a, 1, 0));
}

// Read a bit operand: a non-negative integer below 2^48.
std::optional<std::uint64_t> bit_operand(double x) {
    if (x < 0 || x != std::floor(x) || x >= 281474976710656.0)
        return std::nullopt;
    return static_cast<std::uint64_t>(x);
}

template <std::uint64_t (*f)(std::uint64_t, std::uint64_t)>
value bit_op(call_context &, args_t a) {
    const auto x = bit_operand(a[0].number());
    const auto y = bit_operand(a[1].number());
    if (!x || !y)
        return value(error_code::num);
    return value(static_cast<double>(f(*x, *y)));
}

std::uint64_t bit_and(std::uint64_t x, std::uint64_t y) { return x & y; }
std::uint64_t bit_or(std::uint64_t x, std::uint64_t y) { return x | y; }
std::uint64_t bit_xor(std::uint64_t x, std::uint64_t y) { return x ^ y; }

value bit_shift(double number, double shift) {
    if (number < 0 || std::fabs(shift) > 53)
        return value(error_code::num);
    return checked(std::floor(std::ldexp(std::floor(number),
                                         static_cast<int>(shift))));
}

value erf_of(call_context &, args_t a) {
    const double low = a[0].number();
    if (a.size() > 1 && !a[1].is_empty())
        return checked(std::erf(a[1].number()) - std::erf(low));
    return checked(std::erf(low));
}

const function_row rows[] = {
    {"BIN2DEC", "S", convert_base<2, 10>, "binary to decimal"},
    {"BIN2OCT", "S|f", convert_base<2, 8>, "binary to octal"},
    {"BIN2HEX", "S|f", convert_base<2, 16>, "binary to hexadecimal"},
    {"OCT2DEC", "S", convert_base<8, 10>, "octal to decimal"},
    {"OCT2BIN", "S|f", convert_base<8, 2>, "octal to binary"},
    {"OCT2HEX", "S|f", convert_base<8, 16>, "octal to hexadecimal"},
    {"HEX2DEC", "S", convert_base<16, 10>, "hexadecimal to decimal"},
    {"HEX2BIN", "S|f", convert_base<16, 2>, "hexadecimal to binary"},
    {"HEX2OCT", "S|f", convert_base<16, 8>, "hexadecimal to octal"},
    {"DEC2BIN", "f|f", from_decimal<2>, "decimal to binary"},
    {"DEC2OCT", "f|f", from_decimal<8>, "decimal to octal"},
    {"DEC2HEX", "f|f", from_decimal<16>, "decimal to hexadecimal"},
    {"BITAND", "ff", bit_op<bit_and>, "bitwise and"},
    {"BITOR", "ff", bit_op<bit_or>, "bitwise or"},
    {"BITXOR", "ff", bit_op<bit_xor>, "bitwise exclusive or"},
    {"BITLSHIFT", "ff",
     [](call_context &, args_t a) -> value {
         return bit_shift(a[0].number(), a[1].number());
     },
     "shift bits left"},
    {"BITRSHIFT", "ff",
     [](call_context &, args_t a) -> value {
         return bit_shift(a[0].number(), -a[1].number());
     },
     "shift bits right"},
    {"DELTA", "f|f",
     [](call_context &, args_t a) -> value {
         return value(a[0].number() == number_or(a, 1, 0) ? 1.0 : 0.0);
     },
     "1 when two numbers are equal"},
    {"GESTEP", "f|f",
     [](call_context &, args_t a) -> value {
         return value(a[0].number() >= number_or(a, 1, 0) ? 1.0 : 0.0);
     },
     "1 when a number reaches a step"},
    {"ERF", "f|f", erf_of, "the error function"},
    {"ERFC", "f",
     [](call_context &, args_t a) -> value {
         return checked(std::erfc(a[0].number()));
     },
     "the complementary error function"},
};

} // namespace

void register_eng(expr::function_table &table) {
    table.add_rows("Engineering", rows);
}

} // namespace func
