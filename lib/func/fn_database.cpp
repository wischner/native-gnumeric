//
// Database functions over a table with a header row and a criteria
// block: rows of a block are alternatives, columns of a row must all
// hold.
//
// Derived from Gnumeric plugins/fn-database/functions.c,
// Copyright (C) 1998-2002 Jukka-Pekka Iivonen, Morten Welinder.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include <algorithm>
#include <cmath>
#include <numeric>

#include "func/collect.h"
#include "util/text.h"

namespace func {
namespace {

// Return the zero-based column named by a field argument; -1 when none.
int find_field(const value &table, const value &field) {
    if (field.is_number()) {
        const int col = static_cast<int>(field.number()) - 1;
        return col >= 0 && col < table.grid_cols() ? col : -1;
    }
    if (!field.is_string())
        return -1;
    for (int col = 0; col < table.grid_cols(); ++col)
        if (util::equal_nocase(table.grid_at(col, 0).to_text(),
                               field.string()))
            return col;
    return -1;
}

// One column test of one criteria row.
struct field_test {
    int col;
    criterion test;
};

// Read the criteria block; nothing when a heading names no column.
std::optional<std::vector<std::vector<field_test>>>
read_criteria(const value &table, const value &block) {
    std::vector<std::vector<field_test>> alternatives;
    for (int row = 1; row < block.grid_rows(); ++row) {
        std::vector<field_test> tests;
        for (int col = 0; col < block.grid_cols(); ++col) {
            const value cell = block.grid_at(col, row);
            if (cell.is_empty())
                continue;
            const int field = find_field(table, block.grid_at(col, 0));
            if (field < 0)
                return std::nullopt;
            tests.push_back({field, criterion(cell)});
        }
        alternatives.push_back(std::move(tests));
    }
    return alternatives;
}

// Return the values of a field in the rows the criteria select.
std::optional<std::vector<value>> selected(args_t a, bool whole_rows) {
    const value &table = a[0];
    const int field = whole_rows && a[1].is_empty()
                          ? 0
                          : find_field(table, a[1]);
    if (field < 0)
        return std::nullopt;
    const auto alternatives = read_criteria(table, a[2]);
    if (!alternatives)
        return std::nullopt;
    std::vector<value> out;
    for (int row = 1; row < table.grid_rows(); ++row) {
        const bool hit = std::any_of(
            alternatives->begin(), alternatives->end(),
            [&](const std::vector<field_test> &tests) {
                return std::all_of(tests.begin(), tests.end(),
                                   [&](const field_test &item) {
                                       return item.test.matches(
                                           table.grid_at(item.col, row));
                                   });
            });
        if (hit)
            out.push_back(table.grid_at(field, row));
    }
    return out;
}

// Return the numbers among values.
std::vector<double> numbers_of(const std::vector<value> &values) {
    std::vector<double> numbers;
    for (const value &item : values)
        if (item.is_number())
            numbers.push_back(item.number());
    return numbers;
}

value d_count(call_context &, args_t a) {
    const auto values = selected(a, true);
    if (!values)
        return value(error_code::num);
    return a[1].is_empty() ? value(static_cast<double>(values->size()))
                           : value(static_cast<double>(numbers_of(*values).size()));
}

value d_counta(call_context &, args_t a) {
    const auto values = selected(a, true);
    if (!values)
        return value(error_code::num);
    double count = 0;
    for (const value &item : *values)
        if (!item.is_empty())
            ++count;
    return value(count);
}

value d_get(call_context &, args_t a) {
    const auto values = selected(a, false);
    if (!values)
        return value(error_code::num);
    if (values->empty())
        return value(error_code::value);
    return values->size() == 1 ? values->front() : value(error_code::num);
}

// Sum of squares about the mean.
double squares(const std::vector<double> &n) {
    const double mean = std::accumulate(n.begin(), n.end(), 0.0) / n.size();
    double total = 0;
    for (const double x : n)
        total += (x - mean) * (x - mean);
    return total;
}

// One aggregate over the selected numbers.
template <value (*compute)(const std::vector<double> &)>
value on_numbers(call_context &, args_t a) {
    const auto values = selected(a, false);
    if (!values)
        return value(error_code::num);
    return compute(numbers_of(*values));
}

value total(const std::vector<double> &n) {
    return checked(std::accumulate(n.begin(), n.end(), 0.0));
}
value mean(const std::vector<double> &n) {
    return n.empty() ? value(error_code::div0) : value(*total(n).to_number() / n.size());
}
value largest(const std::vector<double> &n) {
    return n.empty() ? value(0.0) : value(*std::max_element(n.begin(), n.end()));
}
value smallest(const std::vector<double> &n) {
    return n.empty() ? value(0.0) : value(*std::min_element(n.begin(), n.end()));
}
value product(const std::vector<double> &n) {
    double result = 1;
    for (const double x : n)
        result *= x;
    return checked(result);
}
value variance(const std::vector<double> &n) {
    return n.size() < 2 ? value(error_code::div0)
                        : checked(squares(n) / (n.size() - 1));
}
value variance_p(const std::vector<double> &n) {
    return n.empty() ? value(error_code::div0) : checked(squares(n) / n.size());
}
value deviation(const std::vector<double> &n) {
    return n.size() < 2 ? value(error_code::div0)
                        : checked(std::sqrt(squares(n) / (n.size() - 1)));
}
value deviation_p(const std::vector<double> &n) {
    return n.empty() ? value(error_code::div0)
                     : checked(std::sqrt(squares(n) / n.size()));
}

const function_row rows[] = {
    {"DSUM", "ASA", on_numbers<total>, "sum over matching rows"},
    {"DAVERAGE", "ASA", on_numbers<mean>, "average over matching rows"},
    {"DMAX", "ASA", on_numbers<largest>, "maximum over matching rows"},
    {"DMIN", "ASA", on_numbers<smallest>, "minimum over matching rows"},
    {"DPRODUCT", "ASA", on_numbers<product>, "product over matching rows"},
    {"DVAR", "ASA", on_numbers<variance>, "sample variance of matching rows"},
    {"DVARP", "ASA", on_numbers<variance_p>,
     "population variance of matching rows"},
    {"DSTDEV", "ASA", on_numbers<deviation>,
     "sample deviation of matching rows"},
    {"DSTDEVP", "ASA", on_numbers<deviation_p>,
     "population deviation of matching rows"},
    {"DCOUNT", "ASA", d_count, "count numbers in matching rows"},
    {"DCOUNTA", "ASA", d_counta, "count values in matching rows"},
    {"DGET", "ASA", d_get, "the one value of a matching row"},
};

} // namespace

void register_database(expr::function_table &table) {
    table.add_rows("Database", rows);
}

} // namespace func
