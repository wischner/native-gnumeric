//
// Number theory, by trial division: spreadsheet arguments stay small.
//
// Derived from Gnumeric plugins/fn-numtheory/numtheory.c,
// Copyright (C) 2003 Brian J. Murrell, Morten Welinder.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include <cmath>
#include <numbers>
#include <cstdint>
#include <vector>

#include "func/collect.h"

namespace func {
namespace {

constexpr double largest_argument = 1e12;
constexpr double most_primes = 1e6;

// Read a positive integer argument.
std::optional<std::uint64_t> whole(const value &item) {
    const double number = std::floor(item.number());
    if (number < 1 || number > largest_argument)
        return std::nullopt;
    return static_cast<std::uint64_t>(number);
}

// One prime factor and its power.
struct factor {
    std::uint64_t prime;
    int power;
};

// Return a number's prime factors.
std::vector<factor> factorize(std::uint64_t n) {
    std::vector<factor> found;
    for (std::uint64_t p = 2; p * p <= n; ++p) {
        if (n % p != 0)
            continue;
        int power = 0;
        while (n % p == 0) {
            n /= p;
            ++power;
        }
        found.push_back({p, power});
    }
    if (n > 1)
        found.push_back({n, 1});
    return found;
}

// Return whether a number is prime.
bool is_prime(std::uint64_t n) {
    if (n < 2)
        return false;
    for (std::uint64_t p = 2; p * p <= n; ++p)
        if (n % p == 0)
            return false;
    return true;
}

// Wrap a function of a number's factors.
template <double (*f)(const std::vector<factor> &)>
value of_factors(call_context &, args_t a) {
    const auto n = whole(a[0]);
    return n ? checked(f(factorize(*n))) : value(error_code::num);
}

double totient(const std::vector<factor> &factors) {
    double result = 1;
    for (const factor &item : factors)
        result *= std::pow(static_cast<double>(item.prime), item.power - 1) *
                  static_cast<double>(item.prime - 1);
    return result;
}

double divisor_count(const std::vector<factor> &factors) {
    double result = 1;
    for (const factor &item : factors)
        result *= item.power + 1;
    return result;
}

double divisor_sum(const std::vector<factor> &factors) {
    double result = 1;
    for (const factor &item : factors) {
        const auto prime = static_cast<double>(item.prime);
        result *= (std::pow(prime, item.power + 1) - 1) / (prime - 1);
    }
    return result;
}

double moebius(const std::vector<factor> &factors) {
    for (const factor &item : factors)
        if (item.power > 1)
            return 0;
    return factors.size() % 2 == 0 ? 1 : -1;
}

double omega(const std::vector<factor> &factors) {
    return static_cast<double>(factors.size());
}

value nth_prime(call_context &, args_t a) {
    const double index = std::floor(a[0].number());
    if (index < 1 || index > most_primes)
        return value(error_code::num);
    std::uint64_t candidate = 1;
    for (double found = 0; found < index;)
        if (is_prime(++candidate))
            ++found;
    return value(static_cast<double>(candidate));
}

value prime_count(call_context &, args_t a) {
    const double limit = std::floor(a[0].number());
    if (limit < 0 || limit > 1e8)
        return value(error_code::num);
    std::vector<bool> composite(static_cast<std::size_t>(limit) + 1, false);
    double count = 0;
    for (std::size_t i = 2; i <= static_cast<std::size_t>(limit); ++i) {
        if (composite[i])
            continue;
        ++count;
        for (std::size_t j = i * i; j <= static_cast<std::size_t>(limit);
             j += i)
            composite[j] = true;
    }
    return value(count);
}

value prime_test(call_context &, args_t a) {
    const auto n = whole(a[0]);
    return n ? value(is_prime(*n)) : value(error_code::num);
}

value smallest_factor(call_context &, args_t a) {
    const auto n = whole(a[0]);
    if (!n || *n < 2)
        return value(error_code::num);
    return value(static_cast<double>(factorize(*n).front().prime));
}

const function_row rows[] = {
    {"ITHPRIME", "f", nth_prime, "the i-th prime"},
    {"ISPRIME", "f", prime_test, "whether a number is prime"},
    {"PFACTOR", "f", smallest_factor, "smallest prime factor"},
    {"NT_PI", "f", prime_count, "number of primes up to n"},
    {"NT_PHI", "f", of_factors<totient>, "Euler's totient"},
    {"NT_D", "f", of_factors<divisor_count>, "number of divisors"},
    {"NT_SIGMA", "f", of_factors<divisor_sum>, "sum of divisors"},
    {"NT_MU", "f", of_factors<moebius>, "Moebius function"},
    {"NT_OMEGA", "f", of_factors<omega>, "number of distinct prime factors"},
    {"SINPI", "f",
     [](call_context &, args_t a) -> value {
         return checked(std::sin(std::numbers::pi * std::fmod(a[0].number(), 2.0)));
     },
     "sine of pi times x"},
    {"COSPI", "f",
     [](call_context &, args_t a) -> value {
         return checked(std::cos(std::numbers::pi * std::fmod(a[0].number(), 2.0)));
     },
     "cosine of pi times x"},
    {"POCHHAMMER", "ff",
     [](call_context &, args_t a) -> value {
         return checked(std::tgamma(a[0].number() + a[1].number()) /
                        std::tgamma(a[0].number()));
     },
     "rising factorial"},
};

} // namespace

void register_numtheory(expr::function_table &table) {
    table.add_rows("Number theory", rows);
}

} // namespace func
