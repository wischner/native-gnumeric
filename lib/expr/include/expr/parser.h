//
// The formula parser. A recursive-descent parser over a small scanner
// replaces Gnumeric's yacc grammar; precedence follows parser.y.
//
//      = < > <= >= <>      lowest, left
//      &                   left
//      + -                 left
//      * /                 left
//      ^                   right; (-2)^2, unary binds tighter
//      %                   postfix
//      - + unary           highest
//
// Derived from Gnumeric src/parser.y and src/parse-util.c,
// Copyright (C) 1998-2002 Miguel de Icaza, Jody Goldberg, Morten Welinder.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#ifndef EXPR_PARSER_H
#define EXPR_PARSER_H

#include <string>
#include <string_view>

#include "expr/function.h"
#include "expr/node.h"

namespace expr {

// The outcome of parsing.
struct parse_result {
    // Null on failure.
    node_ptr tree;

    // What went wrong.
    std::string error;

    // Byte offset of the failure.
    std::size_t error_at = 0;
};

// What a formula's text is relative to.
struct parse_pos {
    cell_source *sheet = nullptr;
    cell_pos pos;
    name_scope *scope = nullptr;
};

//
// Parse a formula.
//
// Parameters:
//      text    - The formula, with or without a leading '='.
//      pos     - Its sheet, cell and names.
//
// Returns:
//      A tree, or an error with its place. Unknown functions and names
//      parse, and evaluate to #NAME?, so a file loads whole and saves
//      back what it held.
//
parse_result parse_formula(std::string_view text, const parse_pos &pos);

// Quote a sheet name for a formula if it needs it.
std::string quote_sheet_name(std::string_view name);

} // namespace expr

#endif
