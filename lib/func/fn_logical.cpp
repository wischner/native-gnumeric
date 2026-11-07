//
// Logical functions; those that choose evaluate only what they choose.
//
// Derived from Gnumeric plugins/fn-logical/functions.c and
// src/func-builtin.c,
// Copyright (C) 1998-2002 Jukka-Pekka Iivonen, Morten Welinder.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include "func/collect.h"

namespace func {
namespace {

using expr::eval_mode;

// Fold booleans through AND, OR or XOR.
template <typename step_t>
value fold_logic(call_context &context, nodes_t nodes, bool start,
                 step_t step) {
    bool result = start;
    bool any = false;
    auto failed = for_each_value(
        context, nodes, [&](const value &item) -> std::optional<value> {
            if (item.is_error())
                return item;
            if (item.is_empty() || item.is_string())
                return std::nullopt;
            result = step(result, *item.to_bool());
            any = true;
            return std::nullopt;
        });
    if (failed)
        return *failed;
    return any ? value(result) : value(error_code::value);
}

value logic_and(call_context &context, nodes_t nodes) {
    return fold_logic(context, nodes, true,
                      [](bool a, bool b) { return a && b; });
}
value logic_or(call_context &context, nodes_t nodes) {
    return fold_logic(context, nodes, false,
                      [](bool a, bool b) { return a || b; });
}
value logic_xor(call_context &context, nodes_t nodes) {
    return fold_logic(context, nodes, false,
                      [](bool a, bool b) { return a != b; });
}

value logic_if(call_context &context, nodes_t nodes) {
    if (nodes.empty() || nodes.size() > 3)
        return value(error_code::value);
    const value test = nodes[0]->eval(context.pos, eval_mode::scalar);
    if (test.is_error())
        return test;
    const auto flag = test.to_bool();
    if (!flag)
        return value(error_code::value);
    const std::size_t branch = *flag ? 1 : 2;
    if (branch >= nodes.size())
        return value(*flag);
    if (nodes[branch]->as_constant() &&
        nodes[branch]->as_constant()->is_empty())
        return value(0.0);
    return nodes[branch]->eval(context.pos, context.pos.array
                                                ? eval_mode::any
                                                : eval_mode::scalar);
}

value if_error(call_context &context, nodes_t nodes) {
    if (nodes.size() != 2)
        return value(error_code::value);
    const value first = nodes[0]->eval(context.pos, eval_mode::scalar);
    return first.is_error() ? nodes[1]->eval(context.pos, eval_mode::scalar)
                            : first;
}

value if_na(call_context &context, nodes_t nodes) {
    if (nodes.size() != 2)
        return value(error_code::value);
    const value first = nodes[0]->eval(context.pos, eval_mode::scalar);
    if (first.is_error() && first.error() == error_code::na)
        return nodes[1]->eval(context.pos, eval_mode::scalar);
    return first;
}

value logic_ifs(call_context &context, nodes_t nodes) {
    if (nodes.empty() || nodes.size() % 2 != 0)
        return value(error_code::value);
    for (std::size_t i = 0; i < nodes.size(); i += 2) {
        const value test = nodes[i]->eval(context.pos, eval_mode::scalar);
        if (test.is_error())
            return test;
        const auto flag = test.to_bool();
        if (!flag)
            return value(error_code::value);
        if (*flag)
            return nodes[i + 1]->eval(context.pos, eval_mode::scalar);
    }
    return value(error_code::na);
}

value logic_switch(call_context &context, nodes_t nodes) {
    if (nodes.size() < 3)
        return value(error_code::value);
    const value subject = nodes[0]->eval(context.pos, eval_mode::scalar);
    if (subject.is_error())
        return subject;
    std::size_t i = 1;
    for (; i + 1 < nodes.size(); i += 2) {
        const value candidate = nodes[i]->eval(context.pos, eval_mode::scalar);
        if (candidate.is_error())
            return candidate;
        if (subject.compare(candidate) == val::comparison::equal)
            return nodes[i + 1]->eval(context.pos, eval_mode::scalar);
    }
    if (i < nodes.size())
        return nodes[i]->eval(context.pos, eval_mode::scalar);
    return value(error_code::na);
}

const function_row rows[] = {
    {"NOT", "b",
     [](call_context &, args_t a) -> value { return value(!a[0].flag()); },
     "logical negation"},
    {"TRUE", "",
     [](call_context &, args_t) -> value { return value(true); },
     "the value TRUE"},
    {"FALSE", "",
     [](call_context &, args_t) -> value { return value(false); },
     "the value FALSE"},
};

const variadic_row variadic[] = {
    {"AND", logic_and, "true when all are true"},
    {"OR", logic_or, "true when any is true"},
    {"XOR", logic_xor, "true when an odd number are true"},
    {"IF", logic_if, "choose by a condition"},
    {"IFERROR", if_error, "a fallback for any error"},
    {"IFNA", if_na, "a fallback for #N/A"},
    {"IFS", logic_ifs, "the first true condition"},
    {"SWITCH", logic_switch, "choose by matching a value"},
};

} // namespace

void register_logical(expr::function_table &table) {
    table.add_rows("Logic", rows);
    add_variadic(table, "Logic", variadic);
}

} // namespace func
