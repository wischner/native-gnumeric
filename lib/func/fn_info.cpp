//
// Information functions. These must see errors and blanks unconverted,
// so most take E or ? arguments.
//
// Derived from Gnumeric plugins/fn-info/functions.c,
// Copyright (C) 1998-2002 Jukka-Pekka Iivonen, Jody Goldberg, Morten
// Welinder.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include <cmath>

#include "func/collect.h"

namespace func {
namespace {

using expr::eval_mode;

// The value of a single argument node, unconverted.
value argument(call_context &context, nodes_t nodes) {
    if (nodes.size() != 1)
        return value(error_code::value);
    return nodes[0]->eval(context.pos, eval_mode::scalar);
}

value is_blank(call_context &context, nodes_t nodes) {
    return value(argument(context, nodes).is_empty());
}
value is_error(call_context &context, nodes_t nodes) {
    return value(argument(context, nodes).is_error());
}
value is_err(call_context &context, nodes_t nodes) {
    const value item = argument(context, nodes);
    return value(item.is_error() && item.error() != error_code::na);
}
value is_na(call_context &context, nodes_t nodes) {
    const value item = argument(context, nodes);
    return value(item.is_error() && item.error() == error_code::na);
}
value is_number(call_context &context, nodes_t nodes) {
    return value(argument(context, nodes).is_number());
}
value is_text(call_context &context, nodes_t nodes) {
    return value(argument(context, nodes).is_string());
}
value is_nontext(call_context &context, nodes_t nodes) {
    return value(!argument(context, nodes).is_string());
}
value is_logical(call_context &context, nodes_t nodes) {
    return value(argument(context, nodes).is_bool());
}

value is_ref(call_context &context, nodes_t nodes) {
    if (nodes.size() != 1)
        return value(error_code::value);
    return value(nodes[0]->as_area(context.pos).has_value());
}

value is_formula(call_context &context, nodes_t nodes) {
    if (nodes.size() != 1)
        return value(error_code::value);
    const value item = nodes[0]->eval(context.pos, eval_mode::any);
    if (!item.is_area())
        return value(error_code::ref);
    return value(item.cells().sheet->is_formula(item.cells().range.start));
}

// ISEVEN and ISODD test the integer part.
template <int remainder> value parity(call_context &, args_t a) {
    const double whole = std::trunc(std::fabs(a[0].number()));
    return value(std::fmod(whole, 2) == remainder);
}

value type_of(call_context &context, nodes_t nodes) {
    if (nodes.size() != 1)
        return value(error_code::value);
    const value item = nodes[0]->eval(context.pos, eval_mode::any);
    if (item.is_grid() && (item.grid_cols() > 1 || item.grid_rows() > 1))
        return value(64.0);
    const value scalar = item.is_grid() ? item.grid_at(0, 0) : item;
    if (scalar.is_string())
        return value(2.0);
    if (scalar.is_bool())
        return value(4.0);
    if (scalar.is_error())
        return value(16.0);
    return value(1.0);
}

value error_type(call_context &, args_t a) {
    if (!a[0].is_error())
        return value(error_code::na);
    switch (a[0].error()) {
    case error_code::null:
        return value(1.0);
    case error_code::div0:
        return value(2.0);
    case error_code::value:
        return value(3.0);
    case error_code::ref:
        return value(4.0);
    case error_code::name:
        return value(5.0);
    case error_code::num:
        return value(6.0);
    case error_code::na:
        return value(7.0);
    default:
        return value(error_code::na);
    }
}

value number_of(call_context &, args_t a) {
    if (a[0].is_number())
        return a[0];
    if (a[0].is_bool())
        return value(a[0].flag() ? 1.0 : 0.0);
    if (a[0].is_error())
        return a[0];
    return value(0.0);
}

const function_row rows[] = {
    {"ISEVEN", "f", parity<0>, "whether a number is even"},
    {"ISODD", "f", parity<1>, "whether a number is odd"},
    {"ERROR.TYPE", "E", error_type, "the number of an error"},
    {"N", "E", number_of, "a value as a number"},
    {"NA", "",
     [](call_context &, args_t) -> value { return value(error_code::na); },
     "the error #N/A"},
    {"ERROR", "s",
     [](call_context &, args_t a) -> value {
         const auto code = val::parse_error(a[0].string());
         return value(code ? *code : error_code::value);
     },
     "an error from its text"},
};

const variadic_row variadic[] = {
    {"ISBLANK", is_blank, "whether a value is empty"},
    {"ISERROR", is_error, "whether a value is an error"},
    {"ISERR", is_err, "whether a value is an error other than #N/A"},
    {"ISNA", is_na, "whether a value is #N/A"},
    {"ISNUMBER", is_number, "whether a value is a number"},
    {"ISTEXT", is_text, "whether a value is text"},
    {"ISNONTEXT", is_nontext, "whether a value is not text"},
    {"ISLOGICAL", is_logical, "whether a value is boolean"},
    {"ISREF", is_ref, "whether an argument is a reference"},
    {"ISFORMULA", is_formula, "whether a cell holds a formula"},
    {"TYPE", type_of, "the kind of a value"},
};

} // namespace

void register_info(expr::function_table &table) {
    table.add_rows("Information", rows);
    add_variadic(table, "Information", variadic);
}

} // namespace func
