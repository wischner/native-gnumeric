//
// Lookup and reference functions.
//
// Derived from Gnumeric plugins/fn-lookup/functions.c,
// Copyright (C) 1998-2004 Jukka-Pekka Iivonen, Michael Meeks,
// Jody Goldberg, Morten Welinder.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include <algorithm>
#include <cmath>

#include "expr/parser.h"
#include "func/collect.h"

namespace func {
namespace {

using expr::eval_mode;
using val::area;
using val::cell_range;
using val::comparison;

// Compare for lookup: only values of one kind are comparable.
std::optional<comparison> lookup_compare(const value &key,
                                         const value &item) {
    if (item.is_empty())
        return std::nullopt;
    const bool same = (key.is_numeric() && item.is_numeric()) ||
                      (key.is_string() && item.is_string());
    if (!same)
        return std::nullopt;
    return key.compare(item);
}

// Report whether an item equals a key, with wildcards for strings.
bool key_matches(const value &key, const value &item, bool wildcards) {
    if (wildcards && key.is_string() && item.is_string())
        return wildcard_match(key.string(), item.string());
    return lookup_compare(key, item) == comparison::equal;
}

// Find a key in an exact linear scan; -1 when absent.
template <typename read_t>
int find_exact(const value &key, int count, read_t read, bool wildcards) {
    for (int i = 0; i < count; ++i)
        if (key_matches(key, read(i), wildcards))
            return i;
    return -1;
}

// Binary search for the last item not above the key; -1 when none.
template <typename read_t>
int find_sorted(const value &key, int count, read_t read) {
    int low = 0;
    int high = count - 1;
    int best = -1;
    while (low <= high) {
        int middle = (low + high) / 2;
        // Blanks and other types step aside toward the low end.
        int probe = middle;
        std::optional<comparison> order;
        while (probe >= low && !(order = lookup_compare(key, read(probe))))
            --probe;
        if (!order) {
            low = middle + 1;
            continue;
        }
        if (*order == comparison::less) {
            high = probe - 1;
        } else {
            best = probe;
            low = middle + 1;
        }
    }
    return best;
}

// Find the first item not above the key in a descending list.
template <typename read_t>
int find_descending(const value &key, int count, read_t read) {
    int best = -1;
    for (int i = 0; i < count; ++i) {
        const auto order = lookup_compare(key, read(i));
        if (!order)
            continue;
        if (*order == comparison::greater)
            break;
        best = i;
    }
    return best;
}

value v_lookup(call_context &, args_t a) {
    if (a[0].is_error())
        return a[0];
    if (a[0].is_empty())
        return value(error_code::na);
    const value &table = a[1];
    const int col = static_cast<int>(a[2].number());
    if (col < 1)
        return value(error_code::value);
    if (col > table.grid_cols())
        return value(error_code::ref);
    const bool approximate = flag_or(a, 3, true);
    const auto read = [&](int row) { return table.grid_at(0, row); };
    const int row = approximate ? find_sorted(a[0], table.grid_rows(), read)
                                : find_exact(a[0], table.grid_rows(), read,
                                             true);
    return row < 0 ? value(error_code::na) : table.grid_at(col - 1, row);
}

value h_lookup(call_context &, args_t a) {
    if (a[0].is_error())
        return a[0];
    if (a[0].is_empty())
        return value(error_code::na);
    const value &table = a[1];
    const int row = static_cast<int>(a[2].number());
    if (row < 1)
        return value(error_code::value);
    if (row > table.grid_rows())
        return value(error_code::ref);
    const bool approximate = flag_or(a, 3, true);
    const auto read = [&](int col) { return table.grid_at(col, 0); };
    const int col = approximate ? find_sorted(a[0], table.grid_cols(), read)
                                : find_exact(a[0], table.grid_cols(), read,
                                             true);
    return col < 0 ? value(error_code::na) : table.grid_at(col, row - 1);
}

// Read one element of a row or a column by index.
value line_at(const value &line, int index) {
    return line.grid_cols() > 1 ? line.grid_at(index, 0)
                                : line.grid_at(0, index);
}

// Return a line's length.
int line_size(const value &line) {
    return std::max(line.grid_cols(), line.grid_rows());
}

value match_of(call_context &, args_t a) {
    if (a[0].is_error())
        return a[0];
    const value &line = a[1];
    if (line.grid_cols() > 1 && line.grid_rows() > 1)
        return value(error_code::na);
    const int type = static_cast<int>(number_or(a, 2, 1));
    const auto read = [&](int i) { return line_at(line, i); };
    const int size = line_size(line);
    const int index = type == 0   ? find_exact(a[0], size, read, true)
                      : type > 0 ? find_sorted(a[0], size, read)
                                 : find_descending(a[0], size, read);
    return index < 0 ? value(error_code::na) : value(index + 1);
}

value lookup_of(call_context &, args_t a) {
    if (a[0].is_error())
        return a[0];
    const value &source = a[1];
    bool by_column = source.grid_rows() >= source.grid_cols();
    const value line = source;
    const auto read = [&](int i) {
        return by_column ? line.grid_at(0, i) : line.grid_at(i, 0);
    };
    const int size = by_column ? source.grid_rows() : source.grid_cols();
    const int index = find_sorted(a[0], size, read);
    if (index < 0)
        return value(error_code::na);
    if (a.size() > 2 && !a[2].is_empty()) {
        if (index >= line_size(a[2]))
            return value(error_code::na);
        return line_at(a[2], index);
    }
    return by_column ? source.grid_at(source.grid_cols() - 1, index)
                     : source.grid_at(index, source.grid_rows() - 1);
}

value x_lookup(call_context &, args_t a) {
    if (a[0].is_error())
        return a[0];
    const value &keys = a[1];
    const auto read = [&](int i) { return line_at(keys, i); };
    const int index = find_exact(a[0], line_size(keys), read, false);
    if (index < 0)
        return a.size() > 3 && !a[3].is_empty() ? a[3]
                                                : value(error_code::na);
    if (index >= line_size(a[2]))
        return value(error_code::na);
    return line_at(a[2], index);
}

value index_of(call_context &, args_t a) {
    const value &grid = a[0];
    const int cols = grid.grid_cols();
    const int rows = grid.grid_rows();
    int row = static_cast<int>(number_or(a, 1, 0));
    int col = static_cast<int>(number_or(a, 2, 0));
    // One index into a line selects along that line.
    if (a.size() < 3 && rows == 1 && cols > 1) {
        col = row;
        row = 1;
    }
    if (row < 0 || col < 0 || row > rows || col > cols)
        return value(error_code::ref);
    if (grid.is_area()) {
        const area &cells = grid.cells();
        cell_range target = cells.range;
        if (row > 0)
            target.start.row = target.end.row = cells.range.start.row + row - 1;
        if (col > 0)
            target.start.col = target.end.col = cells.range.start.col + col - 1;
        return value(area{cells.sheet, target});
    }
    if (!grid.is_array())
        return grid;
    if (row == 0 && rows > 1)
        return value(error_code::ref);
    if (col == 0 && cols > 1)
        return value(error_code::ref);
    return grid.grid_at(std::max(col, 1) - 1, std::max(row, 1) - 1);
}

value column_of(call_context &context, args_t a) {
    if (a.empty() || a[0].is_empty())
        return value(context.pos.pos.col + 1);
    if (!a[0].is_area())
        return value(error_code::value);
    return value(a[0].cells().range.start.col + 1);
}

value row_of(call_context &context, args_t a) {
    if (a.empty() || a[0].is_empty())
        return value(context.pos.pos.row + 1);
    if (!a[0].is_area())
        return value(error_code::value);
    return value(a[0].cells().range.start.row + 1);
}

value address_of(call_context &, args_t a) {
    const int row = static_cast<int>(a[0].number());
    const int col = static_cast<int>(a[1].number());
    const int mode = static_cast<int>(number_or(a, 2, 1));
    const bool a1 = flag_or(a, 3, true);
    if (row < 1 || col < 1 || mode < 1 || mode > 4)
        return value(error_code::value);
    const bool row_absolute = mode == 1 || mode == 2;
    const bool col_absolute = mode == 1 || mode == 3;
    std::string out;
    if (a.size() > 4 && !a[4].is_empty())
        out = expr::quote_sheet_name(a[4].to_text()) + "!";
    if (a1) {
        out += (col_absolute ? "$" : "") + val::col_name(col - 1);
        out += (row_absolute ? "$" : "") + std::to_string(row);
    } else {
        out += "R" + (row_absolute ? std::to_string(row)
                                   : "[" + std::to_string(row) + "]");
        out += "C" + (col_absolute ? std::to_string(col)
                                   : "[" + std::to_string(col) + "]");
    }
    return value(out);
}

value indirect(call_context &context, args_t a) {
    const expr::parse_pos where{context.pos.sheet, context.pos.pos,
                                context.pos.scope};
    const expr::parse_result parsed =
        expr::parse_formula(a[0].string(), where);
    if (!parsed.tree)
        return value(error_code::ref);
    const auto cells = parsed.tree->as_area(context.pos);
    return cells ? value(*cells) : value(error_code::ref);
}

value offset_of(call_context &context, nodes_t nodes) {
    if (nodes.size() < 3 || nodes.size() > 5)
        return value(error_code::value);
    const value base = nodes[0]->eval(context.pos, eval_mode::any);
    if (base.is_error())
        return base;
    if (!base.is_area())
        return value(error_code::value);
    int numbers[4] = {0, 0, 0, 0};
    for (std::size_t i = 1; i < nodes.size(); ++i) {
        const value item = nodes[i]->eval(context.pos, eval_mode::scalar);
        if (item.is_error())
            return item;
        if (item.is_empty())
            numbers[i - 1] = i <= 2 ? 0 : -1;
        else {
            const auto number = item.to_number();
            if (!number)
                return value(error_code::value);
            numbers[i - 1] = static_cast<int>(*number);
        }
    }
    const area &cells = base.cells();
    const int height = nodes.size() > 3 && numbers[2] >= 0
                           ? numbers[2]
                           : cells.range.rows();
    const int width = nodes.size() > 4 && numbers[3] >= 0
                          ? numbers[3]
                          : cells.range.cols();
    if (height < 1 || width < 1)
        return value(error_code::ref);
    const int row = cells.range.start.row + numbers[0];
    const int col = cells.range.start.col + numbers[1];
    if (row < 0 || col < 0 || row + height > cells.sheet->row_count() ||
        col + width > cells.sheet->col_count())
        return value(error_code::ref);
    return value(area{cells.sheet,
                      {{col, row}, {col + width - 1, row + height - 1}}});
}

value choose(call_context &context, nodes_t nodes) {
    if (nodes.size() < 2)
        return value(error_code::value);
    const value which = nodes[0]->eval(context.pos, eval_mode::scalar);
    if (which.is_error())
        return which;
    const auto number = which.to_number();
    if (!number || *number < 1 ||
        static_cast<std::size_t>(*number) >= nodes.size())
        return value(error_code::value);
    return nodes[static_cast<std::size_t>(*number)]->eval(context.pos,
                                                          eval_mode::any);
}

value transpose(call_context &, args_t a) {
    const int cols = a[0].grid_cols();
    const int rows = a[0].grid_rows();
    auto result = std::make_shared<val::array_value>(rows, cols);
    for (int row = 0; row < rows; ++row)
        for (int col = 0; col < cols; ++col)
            result->set(row, col, a[0].grid_at(col, row));
    return value(std::shared_ptr<const val::array_value>(std::move(result)));
}

value hyperlink(call_context &, args_t a) {
    return a.size() > 1 && !a[1].is_empty() ? a[1] : a[0];
}

value formula_text(call_context &, args_t a) {
    if (!a[0].is_area())
        return value(error_code::value);
    const area &cells = a[0].cells();
    const auto text = cells.sheet->formula_text(cells.range.start);
    return text ? value(*text) : value(error_code::na);
}

const function_row lookup_rows[] = {
    {"VLOOKUP", "EAf|b", v_lookup, "search the first column of a table"},
    {"HLOOKUP", "EAf|b", h_lookup, "search the first row of a table"},
    {"LOOKUP", "EA|A", lookup_of, "search a sorted line"},
    {"XLOOKUP", "EAA|E", x_lookup, "search a line, return from another"},
    {"MATCH", "EA|f", match_of, "position of a value in a line"},
    {"INDEX", "A|ff", index_of, "a cell of a table"},
    {"COLUMN", "|?", column_of, "column of a reference"},
    {"ROW", "|?", row_of, "row of a reference"},
    {"COLUMNS", "A",
     [](call_context &, args_t a) -> value { return value(a[0].grid_cols()); },
     "columns in a reference"},
    {"ROWS", "A",
     [](call_context &, args_t a) -> value { return value(a[0].grid_rows()); },
     "rows in a reference"},
    {"ADDRESS", "ff|fbS", address_of, "a reference as text"},
    {"INDIRECT", "s|b", indirect, "the cells a text names"},
    {"TRANSPOSE", "A", transpose, "swap rows and columns"},
    {"HYPERLINK", "s|S", hyperlink, "text of a link"},
    {"FORMULATEXT", "?", formula_text, "the formula of a cell"},
    {"AREAS", "?",
     [](call_context &, args_t a) -> value {
         return a[0].is_area() ? value(1.0) : value(error_code::value);
     },
     "areas in a reference"},
};

const variadic_row variadic[] = {
    {"OFFSET", offset_of, "cells at an offset from a reference"},
    {"CHOOSE", choose, "a value picked by index"},
};

} // namespace

void register_lookup(expr::function_table &table) {
    table.add_rows("Lookup", lookup_rows);
    add_variadic(table, "Lookup", variadic);
    for (const std::string_view name : {"INDIRECT", "OFFSET"}) {
        expr::function_def definition = *table.find(name);
        definition.is_volatile = true;
        table.add(definition);
    }
}

} // namespace func
