//
// Expression trees. Gnumeric's GnmExpr tagged union becomes a class per
// operator, each evaluating and printing itself; shared immutable nodes
// replace reference counting by hand. A cell reference keeps relative
// parts as offsets from the evaluating cell, so one tree serves every
// cell of a filled column, as Gnumeric's shared expressions do.
//
// Derived from Gnumeric src/expr.c and expr-impl.h,
// Copyright (C) 2001-2006 Jody Goldberg, 1998-2000 Miguel de Icaza,
// 2000-2018 Morten Welinder.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#ifndef EXPR_NODE_H
#define EXPR_NODE_H

#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "val/coords.h"
#include "val/value.h"

namespace expr {

using val::cell_pos;
using val::cell_range;
using val::cell_source;
using val::value;

class node;
class name_scope;
struct function_def;

// A shared immutable tree.
using node_ptr = std::shared_ptr<const node>;

// One end of a reference.
struct cell_ref {
    // Null means the sheet of the formula.
    cell_source *sheet = nullptr;
    int col = 0;            // An offset when col_relative.
    int row = 0;            // An offset when row_relative.
    bool col_relative = true;
    bool row_relative = true;

    friend bool operator==(const cell_ref &, const cell_ref &) = default;

    // Return the absolute position seen from a cell, wrapped to a size.
    cell_pos resolve(cell_pos from, int cols, int rows) const;

    // Return a reference to a position, relative to a cell where marked.
    static cell_ref make(cell_pos target, cell_pos from, bool col_relative,
                         bool row_relative, cell_source *sheet = nullptr);
};

// Where an expression evaluates.
struct eval_pos {
    cell_source *sheet = nullptr;
    cell_pos pos;
    name_scope *scope = nullptr;

    // Whether this evaluates an array formula.
    bool array = false;
};

// Where an expression prints.
struct print_pos {
    const cell_source *sheet = nullptr;
    cell_pos pos;
};

// How the result of an evaluation may be shaped.
enum class eval_mode {
    scalar,     // Areas intersect implicitly; arrays give their first element.
    any         // Areas and arrays are returned whole.
};

// A named expression; its definition can change after references exist.
struct named_expr {
    std::string name;

    // Null for a workbook name.
    cell_source *sheet = nullptr;

    // Null while the name is only referenced, never defined.
    node_ptr definition;

    // The cell relative references in the definition are measured from.
    cell_pos origin;
};

// Maps one reference while a structural edit rewrites a formula.
class ref_mapper {
public:
    virtual ~ref_mapper() = default;

    //
    // Map one referenced rectangle.
    //
    // Parameters:
    //      sheet       - Referenced sheet.
    //      range       - Rectangle before the edit.
    //
    // Returns:
    //      The rectangle after the edit, or nothing when it is deleted.
    //
    virtual std::optional<cell_range> map_range(cell_source *sheet,
                                                cell_range range) = 0;

    // Return where the formula's own cell moves.
    virtual cell_pos map_origin(cell_pos pos) = 0;

    // Report whether a sheet has been deleted.
    virtual bool sheet_deleted(cell_source *sheet) {
        (void)sheet;
        return false;
    }
};

// Receives what an expression reads.
class dependency_sink {
public:
    virtual ~dependency_sink() = default;

    // A rectangle of a sheet is read.
    virtual void depends_on(cell_source *sheet, cell_range range) = 0;

    // The result can change with no cell changing.
    virtual void depends_on_time() = 0;
};

// One node of an expression tree.
class node {
public:
    virtual ~node() = default;

    // Calculate the value at a position.
    virtual value eval(const eval_pos &pos, eval_mode mode) const = 0;

    // Append the formula text.
    virtual void print(std::string &out, const print_pos &pos) const = 0;

    // Report what this subtree reads, seen from a position.
    virtual void collect(dependency_sink &sink, cell_source *sheet,
                         cell_pos pos) const = 0;

    //
    // Return this subtree after a structural edit.
    //
    // Parameters:
    //      self    - Shared pointer to this node.
    //      mapper  - The edit.
    //      sheet   - The formula's sheet.
    //      from    - The formula's position before the edit.
    //
    // Returns:
    //      self when nothing changed, so unchanged formulas stay shared.
    //
    virtual node_ptr rewrite(const node_ptr &self, ref_mapper &mapper,
                             cell_source *sheet, cell_pos from) const = 0;

    // Return the area this node names, if it is a plain reference.
    virtual std::optional<val::area> as_area(const eval_pos &pos) const {
        (void)pos;
        return std::nullopt;
    }

    // Return the constant held, if this is a constant.
    virtual const value *as_constant() const { return nullptr; }

    // Return the function called, if this is a call.
    virtual const function_def *as_call() const { return nullptr; }

    // Return the children, for inspection.
    virtual std::vector<node_ptr> children() const { return {}; }
};

// The binary operators.
enum class binary_op {
    add, sub, mul, div, pow, concat, eq, ne, lt, le, gt, ge
};

// The unary operators.
enum class unary_op { neg, plus, percent, paren };

// Make a constant.
node_ptr make_constant(value constant);

// Make a reference to one cell.
node_ptr make_cell(cell_ref ref);

// Make a reference to a rectangle between two corners.
node_ptr make_range(cell_ref first, cell_ref second);

// Make a reference to a named expression.
node_ptr make_name(std::shared_ptr<named_expr> name);

// Make a unary operation.
node_ptr make_unary(unary_op op, node_ptr operand);

// Make a binary operation.
node_ptr make_binary(binary_op op, node_ptr left, node_ptr right);

// Make a call of a known function.
node_ptr make_call(const function_def *function,
                   std::vector<node_ptr> arguments);

// Make a call of a function this build does not provide.
node_ptr make_unknown_call(std::string name,
                           std::vector<node_ptr> arguments);

// Make a parenthesized set of references, (A1,B2).
node_ptr make_set(std::vector<node_ptr> members);

// Make the intersection of two references, the space operator.
node_ptr make_intersection(node_ptr left, node_ptr right);

// Return a formula's text, without the leading '='.
std::string to_text(const node &tree, const print_pos &pos);

// Apply a binary operator to scalars.
value apply_binary(binary_op op, const value &left, const value &right);

// Reduce a grid to one scalar by implicit intersection with a position.
value intersect_scalar(const value &grid, const eval_pos &pos);

} // namespace expr

#endif
