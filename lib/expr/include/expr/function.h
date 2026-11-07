//
// The function table. A function declares its arguments with Gnumeric's
// one-letter spec string, and the call node coerces and checks them, so
// a function body sees only what it asked for.
//
//      f   number
//      b   boolean
//      s   string
//      S   any scalar
//      E   any scalar, errors welcome
//      r   a reference
//      A   an area or an array
//      ?   anything
//      |   the arguments after this are optional
//
// Derived from Gnumeric src/func.c and func.h,
// Copyright (C) 1998-2001 Michael Meeks, Jody Goldberg, Morten Welinder.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#ifndef EXPR_FUNCTION_H
#define EXPR_FUNCTION_H

#include <map>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "expr/node.h"

namespace expr {

// What a function's result suggests for a General cell's format.
enum class auto_format { none, first_arg, date, time, percent, money };

// Hands a function body its surroundings.
struct call_context {
    const eval_pos &pos;
    const function_def &function;
};

// A function of evaluated arguments.
using args_function = value (*)(call_context &, std::span<const value>);

// A function of unevaluated arguments, for lazy or variadic evaluation.
using nodes_function = value (*)(call_context &, std::span<const node_ptr>);

// One spreadsheet function.
struct function_def {
    std::string_view name;
    std::string_view spec;      // Argument letters; unused with nodes.
    args_function args = nullptr;
    nodes_function nodes = nullptr;
    std::string_view category;
    std::string_view summary;
    bool is_volatile = false;   // Recalculates with no input changing.
    auto_format format = auto_format::none;
};

// Lets a family of functions use a shorter table.
struct function_row {
    std::string_view name;
    std::string_view spec;
    args_function args;
    std::string_view summary;
    auto_format format = auto_format::none;
};

// All known functions.
class function_table {
public:
    // Add a function; a later definition of a name replaces an earlier.
    void add(const function_def &definition);

    // Add a family sharing a category.
    void add_rows(std::string_view category,
                  std::span<const function_row> rows);

    // Find a function by name in any case; null when unknown.
    const function_def *find(std::string_view name) const;

    // Return every function in name order.
    std::vector<const function_def *> all() const;

private:
    std::map<std::string, std::unique_ptr<function_def>, std::less<>> known_;
};

// Answers the names a formula mentions.
class name_scope {
public:
    virtual ~name_scope() = default;

    // Find a sheet by name in any case; null when unknown.
    virtual cell_source *find_sheet(std::string_view name) = 0;

    // Find a function; null when unknown.
    virtual const function_def *find_function(std::string_view name) = 0;

    //
    // Find a name visible from a sheet.
    //
    // Parameters:
    //      name    - The name.
    //      from    - Sheet whose local names come first.
    //      create  - Whether to make an undefined placeholder if absent,
    //                so a formula can refer to a name defined later.
    //
    virtual std::shared_ptr<named_expr>
    find_name(std::string_view name, cell_source *from, bool create) = 0;
};

// Return a suggested format for a formula's result, or nothing.
auto_format suggest_format(const node &tree);

} // namespace expr

#endif
