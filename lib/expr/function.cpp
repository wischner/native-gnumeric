//
// The function table and format suggestion.
//
// Derived from Gnumeric src/func.c and src/auto-format.c,
// Copyright (C) 1998-2001 Michael Meeks, Jody Goldberg, Morten Welinder.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include "expr/function.h"

#include "util/text.h"

namespace expr {

void function_table::add(const function_def &definition) {
    known_[util::to_upper(definition.name)] =
        std::make_unique<function_def>(definition);
}

void function_table::add_rows(std::string_view category,
                              std::span<const function_row> rows) {
    for (const function_row &row : rows) {
        function_def definition;
        definition.name = row.name;
        definition.spec = row.spec;
        definition.args = row.args;
        definition.category = category;
        definition.summary = row.summary;
        definition.format = row.format;
        add(definition);
    }
}

const function_def *function_table::find(std::string_view name) const {
    const auto found = known_.find(util::to_upper(name));
    return found == known_.end() ? nullptr : found->second.get();
}

std::vector<const function_def *> function_table::all() const {
    std::vector<const function_def *> result;
    result.reserve(known_.size());
    for (const auto &entry : known_)
        result.push_back(entry.second.get());
    return result;
}

auto_format suggest_format(const node &tree) {
    if (const function_def *function = tree.as_call()) {
        if (function->format != auto_format::first_arg)
            return function->format;
    }
    for (const node_ptr &child : tree.children()) {
        const auto_format found = suggest_format(*child);
        if (found != auto_format::none)
            return found;
    }
    return auto_format::none;
}

} // namespace expr
