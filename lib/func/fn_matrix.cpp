//
// Matrix functions, by Gaussian elimination with partial pivoting.
//
// Derived from Gnumeric plugins/fn-math/functions.c,
// Copyright (C) 1998-2004 Jukka-Pekka Iivonen, Morten Welinder and others.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include <cmath>
#include <utility>

#include "func/collect.h"

namespace func {
namespace {

// Pivot a column; returns false when the matrix is singular.
bool pivot(matrix &m, int column, double &sign, matrix *companion) {
    int best = column;
    for (int row = column + 1; row < m.rows; ++row)
        if (std::fabs(m.at(column, row)) > std::fabs(m.at(column, best)))
            best = row;
    if (m.at(column, best) == 0)
        return false;
    if (best != column) {
        sign = -sign;
        for (int col = 0; col < m.cols; ++col)
            std::swap(m.at(col, column), m.at(col, best));
        if (companion)
            for (int col = 0; col < companion->cols; ++col)
                std::swap(companion->at(col, column),
                          companion->at(col, best));
    }
    return true;
}

value determinant(call_context &, args_t a) {
    value failure;
    auto m = to_matrix(a[0], failure);
    if (!m)
        return failure;
    if (m->cols != m->rows)
        return value(error_code::value);
    double result = 1;
    for (int column = 0; column < m->cols; ++column) {
        if (!pivot(*m, column, result, nullptr))
            return value(0.0);
        const double head = m->at(column, column);
        result *= head;
        for (int row = column + 1; row < m->rows; ++row) {
            const double factor = m->at(column, row) / head;
            for (int col = column; col < m->cols; ++col)
                m->at(col, row) -= factor * m->at(col, column);
        }
    }
    return checked(result);
}

value inverse(call_context &, args_t a) {
    value failure;
    auto m = to_matrix(a[0], failure);
    if (!m)
        return failure;
    if (m->cols != m->rows)
        return value(error_code::value);
    const int size = m->cols;
    matrix result;
    result.cols = result.rows = size;
    result.cells.assign(static_cast<std::size_t>(size) * size, 0);
    for (int i = 0; i < size; ++i)
        result.at(i, i) = 1;
    double sign = 1;
    for (int column = 0; column < size; ++column) {
        if (!pivot(*m, column, sign, &result))
            return value(error_code::num);
        const double head = m->at(column, column);
        for (int col = 0; col < size; ++col) {
            m->at(col, column) /= head;
            result.at(col, column) /= head;
        }
        for (int row = 0; row < size; ++row) {
            if (row == column)
                continue;
            const double factor = m->at(column, row);
            for (int col = 0; col < size; ++col) {
                m->at(col, row) -= factor * m->at(col, column);
                result.at(col, row) -= factor * result.at(col, column);
            }
        }
    }
    return from_matrix(result);
}

value multiply(call_context &, args_t a) {
    value failure;
    const auto x = to_matrix(a[0], failure);
    if (!x)
        return failure;
    const auto y = to_matrix(a[1], failure);
    if (!y)
        return failure;
    if (x->cols != y->rows)
        return value(error_code::value);
    matrix result;
    result.cols = y->cols;
    result.rows = x->rows;
    result.cells.assign(static_cast<std::size_t>(result.cols) * result.rows,
                        0);
    for (int row = 0; row < result.rows; ++row)
        for (int col = 0; col < result.cols; ++col)
            for (int k = 0; k < x->cols; ++k)
                result.at(col, row) += x->at(k, row) * y->at(col, k);
    return from_matrix(result);
}

value identity(call_context &, args_t a) {
    const double size = std::floor(a[0].number());
    if (size < 1 || size > 4096)
        return value(error_code::value);
    matrix result;
    result.cols = result.rows = static_cast<int>(size);
    result.cells.assign(static_cast<std::size_t>(result.cols) * result.rows,
                        0);
    for (int i = 0; i < result.cols; ++i)
        result.at(i, i) = 1;
    return from_matrix(result);
}

const function_row rows[] = {
    {"MDETERM", "A", determinant, "matrix determinant"},
    {"MINVERSE", "A", inverse, "matrix inverse"},
    {"MMULT", "AA", multiply, "matrix product"},
    {"MUNIT", "f", identity, "identity matrix"},
};

} // namespace

void register_matrix(expr::function_table &table) {
    table.add_rows("Mathematics", rows);
}

} // namespace func
