//
// Sums, rounding, integer arithmetic and conditional aggregates.
//
// Derived from Gnumeric plugins/fn-math/functions.c,
// Copyright (C) 1998-2004 Jukka-Pekka Iivonen, Morten Welinder and others.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>

#include "func/collect.h"
#include "func/mathfunc.h"

namespace func {
namespace {

// Return the shared generator of the random functions.
std::mt19937_64 &generator() {
    static std::mt19937_64 engine{std::random_device{}()};
    return engine;
}

value sum_of(const std::vector<double> &numbers) {
    return checked(std::accumulate(numbers.begin(), numbers.end(), 0.0));
}

value product_of(const std::vector<double> &numbers) {
    double result = numbers.empty() ? 0 : 1;
    for (const double number : numbers)
        result *= number;
    return checked(result);
}

value sum_squares(const std::vector<double> &numbers) {
    double result = 0;
    for (const double number : numbers)
        result += number * number;
    return checked(result);
}

value multinomial(const std::vector<double> &numbers) {
    double total = 0;
    double result = 0;
    for (const double number : numbers) {
        if (number < 0)
            return value(error_code::num);
        const double whole = std::floor(number);
        total += whole;
        result -= log_gamma(whole + 1);
    }
    return checked(std::round(std::exp(result + log_gamma(total + 1))));
}

// Fold whole numbers with a binary function.
template <double (*step)(double, double)>
value fold_integers(const std::vector<double> &numbers) {
    if (numbers.empty())
        return value(error_code::num);
    double result = std::floor(numbers[0]);
    for (const double number : numbers) {
        if (number < 0 || number > 9007199254740992.0)
            return value(error_code::num);
        result = step(result, std::floor(number));
    }
    return checked(result);
}

double gcd_step(double a, double b) {
    while (b != 0) {
        const double next = std::fmod(a, b);
        a = b;
        b = next;
    }
    return a;
}

double lcm_step(double a, double b) {
    if (a == 0 || b == 0)
        return 0;
    return a / gcd_step(a, b) * b;
}

// Round to digits with a function of the scaled value.
template <double (*f)(double)> value round_with(call_context &, args_t a) {
    const double digits = std::trunc(number_or(a, 1, 0));
    const double x = a[0].number();
    if (digits >= 0) {
        const double p = std::pow(10.0, digits);
        const double scaled = x * p;
        // Keep a value that already has so few digits unchanged, so
        // binary noise cannot push it past the rounding boundary.
        return checked(std::isfinite(scaled) ? f(scaled) / p : x);
    }
    const double p = std::pow(10.0, -digits);
    return checked(f(x / p) * p);
}

// Nudge values one representation step past a decimal tie, as a human
// reads them: 2.675 rounds to 2.68.
double nudge(double scaled) {
    return scaled + std::copysign(std::fabs(scaled) * 2.3e-16, scaled);
}
double round_half(double x) { return std::round(nudge(x)); }
double round_up(double x) {
    const double y = std::trunc(x);
    return y == x ? x : y + (x > 0 ? 1 : -1);
}
double round_down(double x) { return std::trunc(nudge(x)); }

value floor_multiple(call_context &, args_t a) {
    const double x = a[0].number();
    const double step = number_or(a, 1, x > 0 ? 1 : -1);
    if (x == 0)
        return value(0.0);
    if (step == 0)
        return value(error_code::div0);
    if (x * step < 0)
        return value(error_code::num);
    return checked(std::floor(nudge(x / step)) * step);
}

value ceiling_multiple(call_context &, args_t a) {
    const double x = a[0].number();
    const double step = number_or(a, 1, x > 0 ? 1 : -1);
    if (x == 0)
        return value(0.0);
    if (step == 0)
        return value(0.0);
    if (x * step < 0)
        return value(error_code::num);
    return checked(std::ceil(x / step * (1 - 2.3e-16)) * step);
}

// Round away from zero to a multiple of 2, plus an offset.
value to_parity(double x, double offset) {
    const double magnitude = std::fabs(x);
    double result = std::ceil((magnitude - offset) / 2) * 2 + offset;
    if (result < magnitude)
        result += 2;
    return checked(std::copysign(result, x));
}

value factorial(call_context &, args_t a) {
    const double x = a[0].number();
    if (x < 0)
        return value(error_code::num);
    const double n = std::floor(x);
    if (n > 170)
        return value(error_code::num);
    return checked(std::round(std::tgamma(n + 1)));
}

value double_factorial(call_context &, args_t a) {
    const double x = std::floor(a[0].number());
    if (x < -1)
        return value(error_code::num);
    double result = 1;
    for (double i = x; i > 1; i -= 2)
        result *= i;
    return checked(result);
}

value modulo(call_context &, args_t a) {
    const double x = a[0].number();
    const double y = a[1].number();
    if (y == 0)
        return value(error_code::div0);
    const double r = std::fmod(x, y);
    return checked(r != 0 && (r < 0) != (y < 0) ? r + y : r);
}

value mround(call_context &, args_t a) {
    const double x = a[0].number();
    const double m = a[1].number();
    if (m == 0)
        return value(0.0);
    if (x * m < 0)
        return value(error_code::num);
    return checked(std::round(nudge(x / m)) * m);
}

value roman(call_context &, args_t a) {
    const double x = std::floor(a[0].number());
    if (x < 0 || x >= 4000)
        return value(error_code::value);
    struct numeral {
        int amount;
        const char *text;
    };
    static constexpr numeral numerals[] = {
        {1000, "M"}, {900, "CM"}, {500, "D"}, {400, "CD"}, {100, "C"},
        {90, "XC"},  {50, "L"},   {40, "XL"}, {10, "X"},   {9, "IX"},
        {5, "V"},    {4, "IV"},   {1, "I"}};
    int left = static_cast<int>(x);
    std::string out;
    for (const numeral &entry : numerals)
        while (left >= entry.amount) {
            out += entry.text;
            left -= entry.amount;
        }
    return value(out);
}

value combin(call_context &, args_t a) {
    const double n = std::floor(a[0].number());
    const double k = std::floor(a[1].number());
    return checked(binomial_coefficient(n, k));
}

value combina(call_context &, args_t a) {
    const double n = std::floor(a[0].number());
    const double k = std::floor(a[1].number());
    if (n < 0 || k < 0)
        return value(error_code::num);
    return checked(binomial_coefficient(n + k - 1, k));
}

value fibonacci(call_context &, args_t a) {
    const double n = std::floor(a[0].number());
    if (n < 0 || n > 1476)
        return value(error_code::num);
    double previous = 0;
    double current = n > 0 ? 1 : 0;
    for (int i = 2; i <= n; ++i) {
        const double next = previous + current;
        previous = current;
        current = next;
    }
    return checked(current);
}

value sign_of(call_context &, args_t a) {
    const double x = a[0].number();
    return value(x > 0 ? 1.0 : (x < 0 ? -1.0 : 0.0));
}

value series_sum(call_context &, args_t a) {
    const double x = a[0].number();
    const double n = a[1].number();
    const double m = a[2].number();
    double result = 0;
    double power = 0;
    for (const double coefficient : grid_numbers(a[3])) {
        result += coefficient * std::pow(x, n + power * m);
        power += 1;
    }
    return checked(result);
}

// Pair walkers for the SUMX family.
template <double (*f)(double, double)>
value sum_pairs(call_context &, args_t a) {
    std::vector<double> xs;
    std::vector<double> ys;
    if (auto failure = paired_numbers(a[0], a[1], xs, ys))
        return *failure;
    double result = 0;
    for (std::size_t i = 0; i < xs.size(); ++i)
        result += f(xs[i], ys[i]);
    return checked(result);
}

double x2my2(double x, double y) { return x * x - y * y; }
double x2py2(double x, double y) { return x * x + y * y; }
double xmy2(double x, double y) { return (x - y) * (x - y); }

value sum_product(call_context &context, nodes_t nodes) {
    if (nodes.empty())
        return value(error_code::value);
    std::vector<value> grids;
    for (const expr::node_ptr &node : nodes) {
        grids.push_back(node->eval(context.pos, expr::eval_mode::any));
        if (grids.back().is_error())
            return grids.back();
    }
    const int cols = grids[0].grid_cols();
    const int rows = grids[0].grid_rows();
    for (const value &grid : grids)
        if (grid.grid_cols() != cols || grid.grid_rows() != rows)
            return value(error_code::value);
    double total = 0;
    for (int row = 0; row < rows; ++row)
        for (int col = 0; col < cols; ++col) {
            double product = 1;
            for (const value &grid : grids) {
                const value item = grid.grid_at(col, row);
                if (item.is_error())
                    return item;
                product *= item.is_number() ? item.number() : 0;
            }
            total += product;
        }
    return checked(total);
}

// Walk the cells satisfying a list of range/criterion pairs.
template <typename visit_t>
std::optional<value> for_matches(const std::vector<value> &ranges,
                                 const std::vector<criterion> &tests,
                                 int cols, int rows, visit_t visit) {
    for (int row = 0; row < rows; ++row)
        for (int col = 0; col < cols; ++col) {
            bool all = true;
            for (std::size_t i = 0; i < ranges.size() && all; ++i)
                all = tests[i].matches(ranges[i].grid_at(col, row));
            if (all)
                if (auto stop = visit(col, row))
                    return stop;
        }
    return std::nullopt;
}

// The IFS family: (target, range, criterion, range, criterion...).
template <typename finish_t>
value conditional_many(call_context &context, nodes_t nodes, bool has_target,
                       finish_t finish) {
    const std::size_t first = has_target ? 1 : 0;
    if (nodes.size() < first + 2 || (nodes.size() - first) % 2 != 0)
        return value(error_code::value);
    std::vector<value> ranges;
    std::vector<criterion> tests;
    for (std::size_t i = first; i < nodes.size(); i += 2) {
        ranges.push_back(nodes[i]->eval(context.pos, expr::eval_mode::any));
        tests.emplace_back(
            nodes[i + 1]->eval(context.pos, expr::eval_mode::scalar));
    }
    const int cols = ranges[0].grid_cols();
    const int rows = ranges[0].grid_rows();
    for (const value &range : ranges)
        if (range.grid_cols() != cols || range.grid_rows() != rows)
            return value(error_code::value);
    value target;
    if (has_target) {
        target = nodes[0]->eval(context.pos, expr::eval_mode::any);
        if (target.grid_cols() != cols || target.grid_rows() != rows)
            return value(error_code::value);
    }
    std::vector<double> hits;
    double count = 0;
    auto failed = for_matches(
        ranges, tests, cols, rows,
        [&](int col, int row) -> std::optional<value> {
            ++count;
            if (!has_target)
                return std::nullopt;
            const value item = target.grid_at(col, row);
            if (item.is_error())
                return item;
            if (item.is_number())
                hits.push_back(item.number());
            return std::nullopt;
        });
    if (failed)
        return *failed;
    return finish(hits, count);
}

value finish_sum(const std::vector<double> &hits, double) {
    return sum_of(hits);
}
value finish_average(const std::vector<double> &hits, double) {
    if (hits.empty())
        return value(error_code::div0);
    return checked(std::accumulate(hits.begin(), hits.end(), 0.0) /
                   hits.size());
}
value finish_max(const std::vector<double> &hits, double) {
    return hits.empty() ? value(0.0)
                        : value(*std::max_element(hits.begin(), hits.end()));
}
value finish_min(const std::vector<double> &hits, double) {
    return hits.empty() ? value(0.0)
                        : value(*std::min_element(hits.begin(), hits.end()));
}

// SUMIF(range, criterion, [target]): the target is sized by the range.
template <value (*finish)(const std::vector<double> &, double)>
value conditional_one(call_context &, args_t a) {
    if (!a[0].is_grid())
        return value(error_code::value);
    const criterion test(a[1]);
    const value &range = a[0];
    const value target = a.size() > 2 && !a[2].is_empty() ? a[2] : a[0];
    std::vector<double> hits;
    double count = 0;
    for (int row = 0; row < range.grid_rows(); ++row)
        for (int col = 0; col < range.grid_cols(); ++col) {
            if (!test.matches(range.grid_at(col, row)))
                continue;
            ++count;
            const value item = target.grid_at(col, row);
            if (item.is_error())
                return item;
            if (item.is_number())
                hits.push_back(item.number());
        }
    return finish(hits, count);
}

value count_if(call_context &, args_t a) {
    if (!a[0].is_grid())
        return value(error_code::value);
    const criterion test(a[1]);
    double count = 0;
    for (int row = 0; row < a[0].grid_rows(); ++row)
        for (int col = 0; col < a[0].grid_cols(); ++col)
            if (test.matches(a[0].grid_at(col, row)))
                ++count;
    return value(count);
}

value subtotal(call_context &context, nodes_t nodes) {
    if (nodes.size() < 2)
        return value(error_code::value);
    const value which = nodes[0]->eval(context.pos, expr::eval_mode::scalar);
    if (which.is_error())
        return which;
    const auto selector = which.to_number();
    if (!selector)
        return value(error_code::value);
    int code = static_cast<int>(*selector);
    if (code > 100)
        code -= 100;
    static constexpr std::string_view names[] = {
        "AVERAGE", "COUNT", "COUNTA", "MAX", "MIN", "PRODUCT",
        "STDEV", "STDEVP", "SUM", "VAR", "VARP"};
    if (code < 1 || code > 11)
        return value(error_code::num);
    const expr::function_def *function =
        context.pos.scope->find_function(names[code - 1]);
    call_context inner{context.pos, *function};
    return function->nodes(inner, nodes.subspan(1));
}

const function_row rows[] = {
    {"ABS", "f",
     [](call_context &, args_t a) -> value {
         return value(std::fabs(a[0].number()));
     },
     "absolute value", expr::auto_format::first_arg},
    {"INT", "f",
     [](call_context &, args_t a) -> value {
         return checked(std::floor(a[0].number()));
     },
     "round down to an integer"},
    {"TRUNC", "f|f", round_with<std::trunc>, "truncate to digits"},
    {"ROUND", "f|f", round_with<round_half>, "round to digits"},
    {"ROUNDUP", "f|f", round_with<round_up>, "round away from zero"},
    {"ROUNDDOWN", "f|f", round_with<round_down>, "round toward zero"},
    {"CEIL", "f",
     [](call_context &, args_t a) -> value {
         return checked(std::ceil(a[0].number()));
     },
     "smallest integer not below x"},
    {"CEILING", "f|f", ceiling_multiple, "round up to a multiple"},
    {"FLOOR", "f|f", floor_multiple, "round down to a multiple"},
    {"MROUND", "ff", mround, "round to a multiple"},
    {"EVEN", "f",
     [](call_context &, args_t a) -> value {
         return to_parity(a[0].number(), 0);
     },
     "round away from zero to even"},
    {"ODD", "f",
     [](call_context &, args_t a) -> value {
         return to_parity(a[0].number(), 1);
     },
     "round away from zero to odd"},
    {"SIGN", "f", sign_of, "sign of a number"},
    {"MOD", "ff", modulo, "remainder, with the sign of the divisor"},
    {"QUOTIENT", "ff",
     [](call_context &, args_t a) -> value {
         if (a[1].number() == 0)
             return value(error_code::div0);
         return checked(std::trunc(a[0].number() / a[1].number()));
     },
     "integer portion of a division"},
    {"POWER", "ff|f",
     [](call_context &, args_t a) -> value {
         const double x = a[0].number();
         const double y = a[1].number();
         const double root = number_or(a, 2, 1);
         if (root <= 0 || (x == 0 && y <= 0) ||
             (x < 0 && y / root != std::floor(y / root)))
             return value(error_code::num);
         return checked(std::pow(x, y / root));
     },
     "x raised to y"},
    {"FACT", "f", factorial, "factorial"},
    {"FACTDOUBLE", "f", double_factorial, "double factorial"},
    {"COMBIN", "ff", combin, "combinations"},
    {"COMBINA", "ff", combina, "combinations with repetition"},
    {"FIB", "f", fibonacci, "Fibonacci numbers"},
    {"ROMAN", "f|f", roman, "Roman numerals"},
    {"RAND", "",
     [](call_context &, args_t) -> value {
         return value(std::uniform_real_distribution<double>(0, 1)(
             generator()));
     },
     "random number in [0,1)"},
    {"RANDBETWEEN", "ff",
     [](call_context &, args_t a) -> value {
         const double low = std::ceil(a[0].number());
         const double high = std::floor(a[1].number());
         if (low > high)
             return value(error_code::num);
         return value(std::floor(
             std::uniform_real_distribution<double>(low, high + 1)(
                 generator())));
     },
     "random integer between two limits"},
    {"SERIESSUM", "fffA", series_sum, "sum of a power series"},
    {"SUMX2MY2", "AA", sum_pairs<x2my2>, "sum of x^2-y^2"},
    {"SUMX2PY2", "AA", sum_pairs<x2py2>, "sum of x^2+y^2"},
    {"SUMXMY2", "AA", sum_pairs<xmy2>, "sum of (x-y)^2"},
    {"COUNTIF", "AS", count_if, "count cells meeting a condition"},
    {"SUMIF", "AS|A", conditional_one<finish_sum>,
     "sum cells meeting a condition"},
    {"AVERAGEIF", "AS|A", conditional_one<finish_average>,
     "average cells meeting a condition"},
    {"MAXIF", "AS|A", conditional_one<finish_max>,
     "maximum of cells meeting a condition"},
    {"MINIF", "AS|A", conditional_one<finish_min>,
     "minimum of cells meeting a condition"},
};

const variadic_row variadic[] = {
    {"SUM", aggregate<sum_of>, "sum of the values",
     expr::auto_format::first_arg},
    {"SUMA", aggregate<sum_of, true, true>,
     "sum, counting text as 0 and booleans as numbers"},
    {"PRODUCT", aggregate<product_of>, "product of the values"},
    {"SUMSQ", aggregate<sum_squares>, "sum of squares"},
    {"GCD", aggregate<fold_integers<gcd_step>>, "greatest common divisor"},
    {"LCM", aggregate<fold_integers<lcm_step>>, "least common multiple"},
    {"MULTINOMIAL", aggregate<multinomial>, "multinomial coefficient"},
    {"SUMPRODUCT", sum_product, "sum of element-wise products"},
    {"SUBTOTAL", subtotal, "aggregate selected by number"},
    {"COUNTIFS",
     [](call_context &context, nodes_t nodes) -> value {
         return conditional_many(
             context, nodes, false,
             [](const std::vector<double> &, double count) -> value {
                 return value(count);
             });
     },
     "count cells meeting several conditions"},
    {"SUMIFS",
     [](call_context &context, nodes_t nodes) -> value {
         return conditional_many(context, nodes, true, finish_sum);
     },
     "sum cells meeting several conditions"},
    {"AVERAGEIFS",
     [](call_context &context, nodes_t nodes) -> value {
         return conditional_many(context, nodes, true, finish_average);
     },
     "average cells meeting several conditions"},
    {"MAXIFS",
     [](call_context &context, nodes_t nodes) -> value {
         return conditional_many(context, nodes, true, finish_max);
     },
     "maximum of cells meeting several conditions"},
    {"MINIFS",
     [](call_context &context, nodes_t nodes) -> value {
         return conditional_many(context, nodes, true, finish_min);
     },
     "minimum of cells meeting several conditions"},
};

} // namespace

void register_math(expr::function_table &table) {
    table.add_rows("Mathematics", rows);
    add_variadic(table, "Mathematics", variadic);
    expr::function_def volatile_rand = *table.find("RAND");
    volatile_rand.is_volatile = true;
    table.add(volatile_rand);
    expr::function_def volatile_between = *table.find("RANDBETWEEN");
    volatile_between.is_volatile = true;
    table.add(volatile_between);
}

} // namespace func
