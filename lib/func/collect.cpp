//
// Argument walking, criteria and the shared registration.
//
// Derived from Gnumeric src/collect.c and src/criteria.c,
// Copyright (C) 1998-2001 Jukka-Pekka Iivonen, Morten Welinder.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include "func/collect.h"

#include "func/functions.h"
#include "numfmt/match.h"
#include "util/text.h"
#include "util/utf8.h"

namespace func {
namespace {

// Match a pattern against text, both in folded UTF-32.
bool glob(std::u32string_view pattern, std::u32string_view text) {
    while (!pattern.empty()) {
        const char32_t c = pattern[0];
        if (c == U'*') {
            pattern.remove_prefix(1);
            if (pattern.empty())
                return true;
            for (std::size_t i = 0; i <= text.size(); ++i)
                if (glob(pattern, text.substr(i)))
                    return true;
            return false;
        }
        if (text.empty())
            return false;
        if (c == U'~' && pattern.size() > 1) {
            if (pattern[1] != text[0])
                return false;
            pattern.remove_prefix(2);
        } else if (c == U'?' || c == text[0]) {
            pattern.remove_prefix(1);
        } else {
            return false;
        }
        text.remove_prefix(1);
    }
    return text.empty();
}

} // namespace

std::optional<value>
for_each_value(call_context &context, nodes_t nodes,
               const std::function<std::optional<value>(const value &)> &visit) {
    for (const expr::node_ptr &node : nodes) {
        const value item = node->eval(context.pos, expr::eval_mode::any);
        if (item.is_area()) {
            std::optional<value> stopped;
            const val::area &cells = item.cells();
            cells.sheet->for_each_cell(
                cells.range, [&](val::cell_pos, const value &cell) {
                    if (!stopped)
                        stopped = visit(cell);
                });
            if (stopped)
                return stopped;
        } else if (item.is_array()) {
            const val::array_value &elements = item.elements();
            for (int row = 0; row < elements.rows(); ++row)
                for (int col = 0; col < elements.cols(); ++col)
                    if (auto stopped = visit(elements.at(col, row)))
                        return stopped;
        } else if (auto stopped = visit(item)) {
            return stopped;
        }
    }
    return std::nullopt;
}

std::optional<value> collect_numbers(call_context &context, nodes_t nodes,
                                     const collect_rules &rules,
                                     std::vector<double> &out) {
    return for_each_value(
        context, nodes, [&](const value &item) -> std::optional<value> {
            if (item.is_number())
                out.push_back(item.number());
            else if (item.is_error()) {
                if (!rules.ignore_errors)
                    return item;
            } else if (item.is_bool()) {
                if (rules.bools_as_numbers)
                    out.push_back(item.flag() ? 1 : 0);
            } else if (item.is_string() && rules.strings_as_zero) {
                out.push_back(0);
            }
            return std::nullopt;
        });
}

std::optional<matrix> to_matrix(const value &grid, value &failure) {
    matrix result;
    result.cols = grid.grid_cols();
    result.rows = grid.grid_rows();
    result.cells.resize(static_cast<std::size_t>(result.cols) * result.rows);
    for (int row = 0; row < result.rows; ++row)
        for (int col = 0; col < result.cols; ++col) {
            const value item = grid.grid_at(col, row);
            if (item.is_error()) {
                failure = item;
                return std::nullopt;
            }
            if (!item.is_numeric()) {
                failure = value(error_code::value);
                return std::nullopt;
            }
            result.at(col, row) = *item.to_number();
        }
    return result;
}

value from_matrix(const matrix &source) {
    auto result = std::make_shared<val::array_value>(source.cols, source.rows);
    for (int row = 0; row < source.rows; ++row)
        for (int col = 0; col < source.cols; ++col)
            result->set(col, row, checked(source.at(col, row)));
    return value(std::shared_ptr<const val::array_value>(std::move(result)));
}

std::vector<double> grid_numbers(const value &grid) {
    std::vector<double> numbers;
    for (int row = 0; row < grid.grid_rows(); ++row)
        for (int col = 0; col < grid.grid_cols(); ++col) {
            const value item = grid.grid_at(col, row);
            if (item.is_number())
                numbers.push_back(item.number());
        }
    return numbers;
}

std::optional<value> paired_numbers(const value &a, const value &b,
                                    std::vector<double> &xs,
                                    std::vector<double> &ys) {
    if (a.grid_cols() * a.grid_rows() != b.grid_cols() * b.grid_rows())
        return value(error_code::na);
    const int cols_a = a.grid_cols();
    const int cols_b = b.grid_cols();
    const int count = cols_a * a.grid_rows();
    for (int i = 0; i < count; ++i) {
        const value left = a.grid_at(i % cols_a, i / cols_a);
        const value right = b.grid_at(i % cols_b, i / cols_b);
        if (left.is_error())
            return left;
        if (right.is_error())
            return right;
        if (left.is_number() && right.is_number()) {
            xs.push_back(left.number());
            ys.push_back(right.number());
        }
    }
    return std::nullopt;
}

bool wildcard_match(std::string_view pattern, std::string_view text) {
    const std::u32string p = util::to_utf32(util::to_lower(pattern));
    const std::u32string t = util::to_utf32(util::to_lower(text));
    return glob(p, t);
}

criterion::criterion(const value &source) : operand_(source) {
    if (!source.is_string())
        return;
    std::string_view text = source.string();
    if (text.substr(0, 2) == "<=") {
        op_ = op::le;
        text.remove_prefix(2);
    } else if (text.substr(0, 2) == ">=") {
        op_ = op::ge;
        text.remove_prefix(2);
    } else if (text.substr(0, 2) == "<>") {
        op_ = op::ne;
        text.remove_prefix(2);
    } else if (!text.empty() && text[0] == '=') {
        text.remove_prefix(1);
    } else if (!text.empty() && text[0] == '<') {
        op_ = op::lt;
        text.remove_prefix(1);
    } else if (!text.empty() && text[0] == '>') {
        op_ = op::gt;
        text.remove_prefix(1);
    }
    if (const auto number = numfmt::match_number(text)) {
        operand_ = value(number->value);
        return;
    }
    if (const auto flag = val::parse_boolean(text)) {
        operand_ = value(*flag);
        return;
    }
    if (const auto code = val::parse_error(text)) {
        operand_ = value(*code);
        return;
    }
    operand_ = value(text);
    pattern_ = text;
    wildcard_ = (op_ == op::eq || op_ == op::ne) &&
                text.find_first_of("*?~") != std::string_view::npos;
}

bool criterion::matches(const value &item) const {
    if (item.is_error() && !operand_.is_error())
        return op_ == op::ne;
    if (wildcard_) {
        const bool hit =
            item.is_string() && wildcard_match(pattern_, item.string());
        return op_ == op::eq ? hit : !hit;
    }
    if (operand_.is_string() && operand_.string().empty()) {
        const bool blank = item.is_empty() ||
                           (item.is_string() && item.string().empty());
        return op_ == op::ne ? !blank : blank;
    }
    if (item.is_empty())
        return op_ == op::ne;
    const bool same_kind = (operand_.is_number() && item.is_number()) ||
                           (operand_.is_string() && item.is_string()) ||
                           (operand_.is_bool() && item.is_bool()) ||
                           (operand_.is_error() && item.is_error());
    if (!same_kind) {
        // A string that reads as a number equals that number.
        if (operand_.is_number() && item.is_string()) {
            const auto number = numfmt::match_number(item.string());
            if (number && (op_ == op::eq || op_ == op::ne))
                return (number->value == operand_.number()) == (op_ == op::eq);
        }
        return op_ == op::ne;
    }
    const val::comparison order = item.compare(operand_);
    using val::comparison;
    switch (op_) {
    case op::eq:
        return order == comparison::equal;
    case op::ne:
        return order != comparison::equal;
    case op::lt:
        return order == comparison::less;
    case op::le:
        return order != comparison::greater;
    case op::gt:
        return order == comparison::greater;
    case op::ge:
        return order != comparison::less;
    }
    return false;
}

void add_variadic(expr::function_table &table, std::string_view category,
                  std::span<const variadic_row> rows) {
    for (const variadic_row &row : rows) {
        expr::function_def definition;
        definition.name = row.name;
        definition.nodes = row.nodes;
        definition.category = category;
        definition.summary = row.summary;
        definition.format = row.format;
        table.add(definition);
    }
}

const expr::function_table &builtin_functions() {
    static const expr::function_table table = [] {
        expr::function_table built;
        register_math(built);
        register_trig(built);
        register_stat(built);
        register_stat_dist(built);
        register_stat_fit(built);
        register_string(built);
        register_logical(built);
        register_lookup(built);
        register_date(built);
        register_financial(built);
        register_info(built);
        register_database(built);
        register_eng(built);
        register_matrix(built);
        register_complex(built);
        register_numtheory(built);
        return built;
    }();
    return table;
}

} // namespace func
