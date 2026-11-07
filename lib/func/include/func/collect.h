//
// Gathers the numbers an aggregate reads from its arguments, under the
// rules of each aggregate for strings, booleans and blanks. Also the
// helpers function bodies share.
//
// Derived from Gnumeric src/collect.c and src/criteria.c,
// Copyright (C) 1998-2001 Jukka-Pekka Iivonen, Morten Welinder.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#ifndef FUNC_COLLECT_H
#define FUNC_COLLECT_H

#include <cmath>
#include <functional>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "expr/function.h"

namespace func {

using args_t = std::span<const expr::value>;
using nodes_t = std::span<const expr::node_ptr>;
using expr::call_context;
using expr::function_row;
using val::error_code;
using val::value;

// How an aggregate treats values that are not numbers.
struct collect_rules {
    bool strings_as_zero = false;   // AVERAGEA: text counts as 0.
    bool bools_as_numbers = false;  // AVERAGEA: TRUE counts as 1.
    bool ignore_errors = false;     // AGGREGATE-style tolerance.
};

//
// Visit every value in arguments; areas visit their existing cells.
//
// Parameters:
//      context - The call.
//      nodes   - Argument trees.
//      visit   - Called per value; a value it returns stops the walk.
//
// Returns:
//      What visit returned, or nothing after a full walk.
//
std::optional<value>
for_each_value(call_context &context, nodes_t nodes,
               const std::function<std::optional<value>(const value &)> &visit);

//
// Collect the numbers of arguments.
//
// Returns:
//      The first error met unless ignored, or nothing.
//
std::optional<value> collect_numbers(call_context &context, nodes_t nodes,
                                     const collect_rules &rules,
                                     std::vector<double> &out);

// Return a number result, turning NaN and infinity into #NUM!.
inline value checked(double result) {
    if (!std::isfinite(result))
        return value(error_code::num);
    return value(result);
}

// Return an optional numeric argument or a default.
inline double number_or(args_t args, std::size_t index, double fallback) {
    if (index >= args.size() || args[index].is_empty())
        return fallback;
    return args[index].to_number().value_or(fallback);
}

// Return an optional boolean argument or a default.
inline bool flag_or(args_t args, std::size_t index, bool fallback) {
    if (index >= args.size() || args[index].is_empty())
        return fallback;
    return args[index].to_bool().value_or(fallback);
}

// A numeric matrix read from a grid.
struct matrix {
    int cols = 0;
    int rows = 0;
    std::vector<double> cells;

    double &at(int col, int row) {
        return cells[static_cast<std::size_t>(row) * cols + col];
    }
    double at(int col, int row) const {
        return cells[static_cast<std::size_t>(row) * cols + col];
    }
};

// Read a grid of numbers; nothing with an error stored when it is not.
std::optional<matrix> to_matrix(const value &grid, value &failure);

// Return a matrix as an array value.
value from_matrix(const matrix &source);

// Read the numbers of a grid in row order, skipping non-numbers.
std::vector<double> grid_numbers(const value &grid);

//
// Read two equal-sized grids as paired numbers.
//
// Notes:
//      A pair is dropped when either side is not a number, as Gnumeric
//      does for its paired statistics.
//
std::optional<value> paired_numbers(const value &a, const value &b,
                                    std::vector<double> &xs,
                                    std::vector<double> &ys);

// A condition such as ">=5", "=abc" or "a*".
class criterion {
public:
    // Parse a criterion value.
    explicit criterion(const value &source);

    // Report whether a value satisfies this criterion.
    bool matches(const value &item) const;

private:
    enum class op { eq, ne, lt, le, gt, ge };

    op op_ = op::eq;
    value operand_;
    bool wildcard_ = false;
    std::string pattern_;
};

// Report whether text matches a pattern of * ? and ~ escapes, any case.
bool wildcard_match(std::string_view pattern, std::string_view text);

// The families of functions.
void register_math(expr::function_table &table);
void register_trig(expr::function_table &table);
void register_stat(expr::function_table &table);
void register_stat_dist(expr::function_table &table);
void register_stat_fit(expr::function_table &table);
void register_string(expr::function_table &table);
void register_logical(expr::function_table &table);
void register_lookup(expr::function_table &table);
void register_date(expr::function_table &table);
void register_financial(expr::function_table &table);
void register_info(expr::function_table &table);
void register_database(expr::function_table &table);
void register_eng(expr::function_table &table);
void register_matrix(expr::function_table &table);
void register_complex(expr::function_table &table);
void register_numtheory(expr::function_table &table);

// A variadic function row.
struct variadic_row {
    std::string_view name;
    expr::nodes_function nodes;
    std::string_view summary;
    expr::auto_format format = expr::auto_format::none;
};

// Add variadic functions to a category.
void add_variadic(expr::function_table &table, std::string_view category,
                  std::span<const variadic_row> rows);

// Return an aggregate as a variadic function body.
template <value (*compute)(const std::vector<double> &),
          bool strings_as_zero = false, bool bools_as_numbers = false>
value aggregate(call_context &context, nodes_t nodes) {
    std::vector<double> numbers;
    const collect_rules rules{strings_as_zero, bools_as_numbers, false};
    if (auto failure = collect_numbers(context, nodes, rules, numbers))
        return *failure;
    return compute(numbers);
}

} // namespace func

#endif
