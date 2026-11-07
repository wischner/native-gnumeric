//
// Descriptive statistics.
//
// Derived from Gnumeric plugins/fn-stat/functions.c,
// Copyright (C) 1998-2002 Jukka-Pekka Iivonen, Morten Welinder.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include <algorithm>
#include <cmath>
#include <map>
#include <numeric>

#include "func/collect.h"
#include "func/mathfunc.h"

namespace func {
namespace {

double mean_of(const std::vector<double> &n) {
    return std::accumulate(n.begin(), n.end(), 0.0) / n.size();
}

// Return the sum of squared deviations.
double squares_of(const std::vector<double> &n) {
    const double mean = mean_of(n);
    double total = 0;
    for (const double x : n)
        total += (x - mean) * (x - mean);
    return total;
}

value average(const std::vector<double> &n) {
    return n.empty() ? value(error_code::div0) : checked(mean_of(n));
}
value maximum(const std::vector<double> &n) {
    return n.empty() ? value(0.0) : value(*std::max_element(n.begin(), n.end()));
}
value minimum(const std::vector<double> &n) {
    return n.empty() ? value(0.0) : value(*std::min_element(n.begin(), n.end()));
}
value variance_sample(const std::vector<double> &n) {
    return n.size() < 2 ? value(error_code::div0)
                        : checked(squares_of(n) / (n.size() - 1));
}
value variance_population(const std::vector<double> &n) {
    return n.empty() ? value(error_code::div0)
                     : checked(squares_of(n) / n.size());
}
value stdev_sample(const std::vector<double> &n) {
    return n.size() < 2 ? value(error_code::div0)
                        : checked(std::sqrt(squares_of(n) / (n.size() - 1)));
}
value stdev_population(const std::vector<double> &n) {
    return n.empty() ? value(error_code::div0)
                     : checked(std::sqrt(squares_of(n) / n.size()));
}
value deviations(const std::vector<double> &n) {
    return n.empty() ? value(error_code::num) : checked(squares_of(n));
}
value mean_deviation(const std::vector<double> &n) {
    if (n.empty())
        return value(error_code::num);
    const double mean = mean_of(n);
    double total = 0;
    for (const double x : n)
        total += std::fabs(x - mean);
    return checked(total / n.size());
}
value geometric_mean(const std::vector<double> &n) {
    if (n.empty())
        return value(error_code::num);
    double total = 0;
    for (const double x : n) {
        if (x <= 0)
            return value(error_code::num);
        total += std::log(x);
    }
    return checked(std::exp(total / n.size()));
}
value harmonic_mean(const std::vector<double> &n) {
    if (n.empty())
        return value(error_code::num);
    double total = 0;
    for (const double x : n) {
        if (x <= 0)
            return value(error_code::num);
        total += 1 / x;
    }
    return checked(n.size() / total);
}

// Return the value at a fraction of a sorted list, interpolating.
double fractile(const std::vector<double> &sorted, double k) {
    const double place = k * (sorted.size() - 1);
    const auto low = static_cast<std::size_t>(place);
    if (low + 1 >= sorted.size())
        return sorted.back();
    return sorted[low] + (place - low) * (sorted[low + 1] - sorted[low]);
}

value median(const std::vector<double> &n) {
    if (n.empty())
        return value(error_code::num);
    std::vector<double> sorted = n;
    std::sort(sorted.begin(), sorted.end());
    return checked(fractile(sorted, 0.5));
}
value mode_of(const std::vector<double> &n) {
    std::map<double, int> counts;
    for (const double x : n)
        ++counts[x];
    double best = 0;
    int best_count = 1;
    // The first value of the most frequent, in reading order.
    for (const double x : n)
        if (counts[x] > best_count) {
            best = x;
            best_count = counts[x];
        }
    return best_count > 1 ? value(best) : value(error_code::na);
}
value skewness(const std::vector<double> &n) {
    const double count = static_cast<double>(n.size());
    if (count < 3)
        return value(error_code::div0);
    const double mean = mean_of(n);
    const double sd = std::sqrt(squares_of(n) / (count - 1));
    if (sd == 0)
        return value(error_code::div0);
    double total = 0;
    for (const double x : n)
        total += std::pow((x - mean) / sd, 3);
    return checked(total * count / ((count - 1) * (count - 2)));
}
value skewness_population(const std::vector<double> &n) {
    const double count = static_cast<double>(n.size());
    if (count < 1)
        return value(error_code::div0);
    const double mean = mean_of(n);
    const double sd = std::sqrt(squares_of(n) / count);
    if (sd == 0)
        return value(error_code::div0);
    double total = 0;
    for (const double x : n)
        total += std::pow((x - mean) / sd, 3);
    return checked(total / count);
}
value kurtosis(const std::vector<double> &n) {
    const double count = static_cast<double>(n.size());
    if (count < 4)
        return value(error_code::div0);
    const double mean = mean_of(n);
    const double sd = std::sqrt(squares_of(n) / (count - 1));
    if (sd == 0)
        return value(error_code::div0);
    double total = 0;
    for (const double x : n)
        total += std::pow((x - mean) / sd, 4);
    const double a = count * (count + 1) / ((count - 1) * (count - 2) * (count - 3));
    const double b = 3 * (count - 1) * (count - 1) / ((count - 2) * (count - 3));
    return checked(a * total - b);
}
value kurtosis_population(const std::vector<double> &n) {
    const double count = static_cast<double>(n.size());
    if (count < 1)
        return value(error_code::div0);
    const double mean = mean_of(n);
    const double sd = std::sqrt(squares_of(n) / count);
    if (sd == 0)
        return value(error_code::div0);
    double total = 0;
    for (const double x : n)
        total += std::pow((x - mean) / sd, 4);
    return checked(total / count - 3);
}

// COUNT counts numbers among its values, including dates.
value count_numbers(call_context &context, nodes_t nodes) {
    double count = 0;
    for_each_value(context, nodes,
                   [&](const value &item) -> std::optional<value> {
                       if (item.is_number())
                           ++count;
                       return std::nullopt;
                   });
    return value(count);
}

value count_nonempty(call_context &context, nodes_t nodes) {
    double count = 0;
    for_each_value(context, nodes,
                   [&](const value &item) -> std::optional<value> {
                       if (!item.is_empty())
                           ++count;
                       return std::nullopt;
                   });
    return value(count);
}

value count_blank(call_context &, args_t a) {
    if (!a[0].is_grid())
        return value(error_code::value);
    double count = 0;
    for (int row = 0; row < a[0].grid_rows(); ++row)
        for (int col = 0; col < a[0].grid_cols(); ++col) {
            const value item = a[0].grid_at(col, row);
            if (item.is_empty() || (item.is_string() && item.string().empty()))
                ++count;
        }
    return value(count);
}

// Return the sorted numbers of a grid; empty when none.
std::vector<double> sorted_numbers(const value &grid) {
    std::vector<double> numbers = grid_numbers(grid);
    std::sort(numbers.begin(), numbers.end());
    return numbers;
}

value kth_largest(call_context &, args_t a) {
    const std::vector<double> numbers = sorted_numbers(a[0]);
    const double k = std::ceil(a[1].number());
    if (k < 1 || k > static_cast<double>(numbers.size()))
        return value(error_code::num);
    return value(numbers[numbers.size() - static_cast<std::size_t>(k)]);
}

value kth_smallest(call_context &, args_t a) {
    const std::vector<double> numbers = sorted_numbers(a[0]);
    const double k = std::ceil(a[1].number());
    if (k < 1 || k > static_cast<double>(numbers.size()))
        return value(error_code::num);
    return value(numbers[static_cast<std::size_t>(k) - 1]);
}

value percentile(call_context &, args_t a) {
    const std::vector<double> numbers = sorted_numbers(a[0]);
    const double k = a[1].number();
    if (numbers.empty() || k < 0 || k > 1)
        return value(error_code::num);
    return checked(fractile(numbers, k));
}

value quartile(call_context &, args_t a) {
    const std::vector<double> numbers = sorted_numbers(a[0]);
    const double q = std::floor(a[1].number());
    if (numbers.empty() || q < 0 || q > 4)
        return value(error_code::num);
    return checked(fractile(numbers, q / 4));
}

value percent_rank(call_context &, args_t a) {
    const std::vector<double> numbers = sorted_numbers(a[0]);
    const double x = a[1].number();
    const int significance = static_cast<int>(number_or(a, 2, 3));
    if (numbers.empty() || x < numbers.front() || x > numbers.back() ||
        significance < 1)
        return value(error_code::na);
    const std::size_t count = numbers.size();
    double rank = 0;
    if (count == 1) {
        rank = 1;
    } else {
        std::size_t below = 0;
        while (below < count && numbers[below] < x)
            ++below;
        if (below < count && numbers[below] == x) {
            rank = static_cast<double>(below) / (count - 1);
        } else {
            const double low = numbers[below - 1];
            const double high = numbers[below];
            rank = ((below - 1) + (x - low) / (high - low)) / (count - 1);
        }
    }
    const double scale = std::pow(10.0, significance);
    return value(std::floor(rank * scale) / scale);
}

value rank_of(call_context &, args_t a) {
    const double x = a[0].number();
    const std::vector<double> numbers = grid_numbers(a[1]);
    const bool ascending = number_or(a, 2, 0) != 0;
    double rank = 1;
    bool found = false;
    for (const double item : numbers) {
        found = found || item == x;
        if (ascending ? item < x : item > x)
            rank += 1;
    }
    return found ? value(rank) : value(error_code::na);
}

value trimmed_mean(call_context &, args_t a) {
    const std::vector<double> numbers = sorted_numbers(a[0]);
    const double fraction = a[1].number();
    if (fraction < 0 || fraction >= 1 || numbers.empty())
        return value(error_code::num);
    const auto drop = static_cast<std::size_t>(
        std::floor(numbers.size() * fraction / 2));
    double total = 0;
    for (std::size_t i = drop; i < numbers.size() - drop; ++i)
        total += numbers[i];
    return checked(total / (numbers.size() - 2 * drop));
}

value frequency(call_context &, args_t a) {
    const std::vector<double> data = grid_numbers(a[0]);
    std::vector<double> bins = grid_numbers(a[1]);
    std::sort(bins.begin(), bins.end());
    auto result = std::make_shared<val::array_value>(
        1, static_cast<int>(bins.size()) + 1);
    std::vector<double> counts(bins.size() + 1, 0);
    for (const double x : data) {
        const auto slot = std::lower_bound(bins.begin(), bins.end(), x);
        counts[static_cast<std::size_t>(slot - bins.begin())] += 1;
    }
    for (std::size_t i = 0; i < counts.size(); ++i)
        result->set(0, static_cast<int>(i), value(counts[i]));
    return value(std::shared_ptr<const val::array_value>(std::move(result)));
}

const function_row rows[] = {
    {"COUNTBLANK", "A", count_blank, "count empty cells"},
    {"LARGE", "Af", kth_largest, "the k-th largest value"},
    {"SMALL", "Af", kth_smallest, "the k-th smallest value"},
    {"PERCENTILE", "Af", percentile, "the k-th percentile"},
    {"QUARTILE", "Af", quartile, "a quartile"},
    {"PERCENTRANK", "Af|f", percent_rank, "rank as a percentage"},
    {"RANK", "fA|f", rank_of, "rank of a value in a list"},
    {"TRIMMEAN", "Af", trimmed_mean, "mean of the interior of a list"},
    {"FREQUENCY", "AA", frequency, "frequency distribution as a column"},
};

const variadic_row variadic[] = {
    {"AVERAGE", aggregate<average>, "arithmetic mean",
     expr::auto_format::first_arg},
    {"AVERAGEA", aggregate<average, true, true>,
     "mean, counting text as 0 and booleans as numbers"},
    {"MAX", aggregate<maximum>, "largest value", expr::auto_format::first_arg},
    {"MAXA", aggregate<maximum, true, true>,
     "largest value, counting text and booleans"},
    {"MIN", aggregate<minimum>, "smallest value",
     expr::auto_format::first_arg},
    {"MINA", aggregate<minimum, true, true>,
     "smallest value, counting text and booleans"},
    {"MEDIAN", aggregate<median>, "the median"},
    {"MODE", aggregate<mode_of>, "the most common value"},
    {"COUNT", count_numbers, "count numbers"},
    {"COUNTA", count_nonempty, "count non-empty values"},
    {"VAR", aggregate<variance_sample>, "sample variance"},
    {"VAR.S", aggregate<variance_sample>, "sample variance"},
    {"VARP", aggregate<variance_population>, "population variance"},
    {"VAR.P", aggregate<variance_population>, "population variance"},
    {"VARA", aggregate<variance_sample, true, true>,
     "sample variance, counting text and booleans"},
    {"VARPA", aggregate<variance_population, true, true>,
     "population variance, counting text and booleans"},
    {"STDEV", aggregate<stdev_sample>, "sample standard deviation"},
    {"STDEV.S", aggregate<stdev_sample>, "sample standard deviation"},
    {"STDEVP", aggregate<stdev_population>, "population standard deviation"},
    {"STDEV.P", aggregate<stdev_population>,
     "population standard deviation"},
    {"STDEVA", aggregate<stdev_sample, true, true>,
     "sample deviation, counting text and booleans"},
    {"STDEVPA", aggregate<stdev_population, true, true>,
     "population deviation, counting text and booleans"},
    {"DEVSQ", aggregate<deviations>, "sum of squared deviations"},
    {"AVEDEV", aggregate<mean_deviation>, "mean absolute deviation"},
    {"GEOMEAN", aggregate<geometric_mean>, "geometric mean"},
    {"HARMEAN", aggregate<harmonic_mean>, "harmonic mean"},
    {"SKEW", aggregate<skewness>, "sample skewness"},
    {"SKEWP", aggregate<skewness_population>, "population skewness"},
    {"KURT", aggregate<kurtosis>, "sample excess kurtosis"},
    {"KURTP", aggregate<kurtosis_population>, "population excess kurtosis"},
};

} // namespace

void register_stat(expr::function_table &table) {
    table.add_rows("Statistics", rows);
    add_variadic(table, "Statistics", variadic);
}

} // namespace func
