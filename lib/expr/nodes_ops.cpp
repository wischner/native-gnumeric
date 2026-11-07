//
// The operator nodes, and the arithmetic of scalars.
//
// Derived from Gnumeric src/expr.c,
// Copyright (C) 2001-2006 Jody Goldberg, 1998-2000 Miguel de Icaza,
// 2000-2018 Morten Welinder.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include <array>
#include <cmath>

#include "expr/node.h"

namespace expr {
namespace {

using val::area;
using val::array_value;
using val::comparison;
using val::error_code;

// Return a scalar's number for arithmetic, or the error to return.
std::optional<double> operand_number(const value &item, value &failure) {
    if (item.is_error()) {
        failure = item;
        return std::nullopt;
    }
    const auto number = item.to_number();
    if (!number)
        failure = value(error_code::value);
    return number;
}

// A prefix or postfix operator.
class unary_node final : public node {
public:
    unary_node(unary_op op, node_ptr operand)
        : op_(op), operand_(std::move(operand)) {}

    value eval(const eval_pos &pos, eval_mode mode) const override {
        const value operand = operand_->eval(pos, mode);
        if (op_ == unary_op::paren)
            return operand;
        if (operand.is_grid())
            return map_grid(operand);
        return apply(operand);
    }

    void print(std::string &out, const print_pos &pos) const override {
        switch (op_) {
        case unary_op::neg:
            out.push_back('-');
            operand_->print(out, pos);
            break;
        case unary_op::plus:
            out.push_back('+');
            operand_->print(out, pos);
            break;
        case unary_op::percent:
            operand_->print(out, pos);
            out.push_back('%');
            break;
        case unary_op::paren:
            out.push_back('(');
            operand_->print(out, pos);
            out.push_back(')');
            break;
        }
    }

    void collect(dependency_sink &sink, cell_source *sheet,
                 cell_pos pos) const override {
        operand_->collect(sink, sheet, pos);
    }

    node_ptr rewrite(const node_ptr &self, ref_mapper &mapper,
                     cell_source *sheet, cell_pos from) const override {
        node_ptr operand = operand_->rewrite(operand_, mapper, sheet, from);
        return operand == operand_ ? self
                                   : make_unary(op_, std::move(operand));
    }

    std::optional<area> as_area(const eval_pos &pos) const override {
        return op_ == unary_op::paren ? operand_->as_area(pos) : std::nullopt;
    }

    std::vector<node_ptr> children() const override { return {operand_}; }

private:
    value apply(const value &operand) const {
        if (op_ == unary_op::plus)
            return operand;
        value failure;
        const auto number = operand_number(operand, failure);
        if (!number)
            return failure;
        return op_ == unary_op::neg ? value(-*number) : value(*number / 100);
    }

    value map_grid(const value &grid) const {
        const int cols = grid.grid_cols();
        const int rows = grid.grid_rows();
        auto result = std::make_shared<array_value>(cols, rows);
        for (int row = 0; row < rows; ++row)
            for (int col = 0; col < cols; ++col)
                result->set(col, row, apply(grid.grid_at(col, row)));
        return value(std::shared_ptr<const array_value>(std::move(result)));
    }

    unary_op op_;
    node_ptr operand_;
};

// One spelling of each binary operator.
constexpr std::array<std::string_view, 12> op_text{
    "+", "-", "*", "/", "^", "&", "=", "<>", "<", "<=", ">", ">="};

// An infix operator.
class binary_node final : public node {
public:
    binary_node(binary_op op, node_ptr left, node_ptr right)
        : op_(op), left_(std::move(left)), right_(std::move(right)) {}

    value eval(const eval_pos &pos, eval_mode mode) const override {
        const value left = left_->eval(pos, mode);
        if (left.is_error())
            return left;
        const value right = right_->eval(pos, mode);
        if (!left.is_grid() && !right.is_grid())
            return apply_binary(op_, left, right);
        const int cols = merged_size(left.grid_cols(), right.grid_cols());
        const int rows = merged_size(left.grid_rows(), right.grid_rows());
        auto result = std::make_shared<array_value>(cols, rows);
        for (int row = 0; row < rows; ++row)
            for (int col = 0; col < cols; ++col)
                result->set(col, row, element(left, right, col, row));
        return value(std::shared_ptr<const array_value>(std::move(result)));
    }

    void print(std::string &out, const print_pos &pos) const override {
        left_->print(out, pos);
        out += op_text[static_cast<std::size_t>(op_)];
        right_->print(out, pos);
    }

    void collect(dependency_sink &sink, cell_source *sheet,
                 cell_pos pos) const override {
        left_->collect(sink, sheet, pos);
        right_->collect(sink, sheet, pos);
    }

    node_ptr rewrite(const node_ptr &self, ref_mapper &mapper,
                     cell_source *sheet, cell_pos from) const override {
        node_ptr left = left_->rewrite(left_, mapper, sheet, from);
        node_ptr right = right_->rewrite(right_, mapper, sheet, from);
        if (left == left_ && right == right_)
            return self;
        return make_binary(op_, std::move(left), std::move(right));
    }

    std::vector<node_ptr> children() const override {
        return {left_, right_};
    }

private:
    // A one-element side repeats; otherwise the sides must agree or
    // the overhang shows #N/A.
    static int merged_size(int a, int b) {
        if (a == 1)
            return b;
        if (b == 1)
            return a;
        return std::max(a, b);
    }

    // Return the value of a side at a place, repeating a single element.
    static std::optional<value> side(const value &grid, int col, int row) {
        const int cols = grid.grid_cols();
        const int rows = grid.grid_rows();
        const int x = cols == 1 ? 0 : col;
        const int y = rows == 1 ? 0 : row;
        if (x >= cols || y >= rows)
            return std::nullopt;
        return grid.grid_at(x, y);
    }

    value element(const value &left, const value &right, int col,
                  int row) const {
        const auto a = side(left, col, row);
        const auto b = side(right, col, row);
        if (!a || !b)
            return value(error_code::na);
        return apply_binary(op_, *a, *b);
    }

    binary_op op_;
    node_ptr left_;
    node_ptr right_;
};

} // namespace

node_ptr make_unary(unary_op op, node_ptr operand) {
    return std::make_shared<unary_node>(op, std::move(operand));
}

node_ptr make_binary(binary_op op, node_ptr left, node_ptr right) {
    return std::make_shared<binary_node>(op, std::move(left),
                                         std::move(right));
}

value apply_binary(binary_op op, const value &left, const value &right) {
    if (left.is_error())
        return left;
    if (right.is_error())
        return right;
    switch (op) {
    case binary_op::concat:
        return value(left.to_text() + right.to_text());
    case binary_op::eq:
    case binary_op::ne:
    case binary_op::lt:
    case binary_op::le:
    case binary_op::gt:
    case binary_op::ge: {
        const comparison order = left.compare(right);
        if (order == comparison::mismatch) {
            if (op == binary_op::eq)
                return value(false);
            if (op == binary_op::ne)
                return value(true);
            return value(error_code::value);
        }
        switch (op) {
        case binary_op::eq:
            return value(order == comparison::equal);
        case binary_op::ne:
            return value(order != comparison::equal);
        case binary_op::lt:
            return value(order == comparison::less);
        case binary_op::le:
            return value(order != comparison::greater);
        case binary_op::gt:
            return value(order == comparison::greater);
        default:
            return value(order != comparison::less);
        }
    }
    default:
        break;
    }
    value failure;
    const auto a = operand_number(left, failure);
    if (!a)
        return failure;
    const auto b = operand_number(right, failure);
    if (!b)
        return failure;
    double result = 0;
    switch (op) {
    case binary_op::add:
        result = *a + *b;
        break;
    case binary_op::sub:
        result = *a - *b;
        break;
    case binary_op::mul:
        result = *a * *b;
        break;
    case binary_op::div:
        if (*b == 0)
            return value(error_code::div0);
        result = *a / *b;
        break;
    default:
        if ((*a == 0 && *b <= 0) || (*a < 0 && *b != std::floor(*b)))
            return value(error_code::num);
        result = std::pow(*a, *b);
        break;
    }
    if (!std::isfinite(result))
        return value(error_code::num);
    return value(result);
}

} // namespace expr
