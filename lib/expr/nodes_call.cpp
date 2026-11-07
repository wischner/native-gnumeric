//
// Function calls, argument coercion, reference sets and intersections.
//
// Derived from Gnumeric src/expr.c,
// Copyright (C) 2001-2006 Jody Goldberg, 1998-2000 Miguel de Icaza,
// 2000-2018 Morten Welinder.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include <algorithm>

#include "expr/function.h"

namespace expr {
namespace {

using val::area;
using val::array_value;
using val::error_code;

// Print an argument list.
void print_arguments(std::string &out, const std::vector<node_ptr> &arguments,
                     const print_pos &pos) {
    out.push_back('(');
    for (std::size_t i = 0; i < arguments.size(); ++i) {
        if (i)
            out.push_back(',');
        arguments[i]->print(out, pos);
    }
    out.push_back(')');
}

// Coerce and check call arguments, then invoke the function.
class argument_binder {
public:
    argument_binder(const function_def &function, const eval_pos &pos)
        : function_(function), pos_(pos) {
        const std::string_view spec = function.spec;
        const auto bar = spec.find('|');
        required_ = bar == std::string_view::npos ? spec.size() : bar;
        for (const char letter : spec)
            if (letter != '|')
                letters_.push_back(letter);
    }

    // Return whether a count of arguments fits the spec.
    bool accepts(std::size_t count) const {
        return count >= required_ && count <= letters_.size();
    }

    // Evaluate an argument as its letter wants it.
    value evaluate(const node &argument, std::size_t index) const {
        const char letter = letters_[index];
        if (letter == 'A' || letter == '?' || letter == 'r')
            return argument.eval(pos_, eval_mode::any);
        return argument.eval(pos_, pos_.array ? eval_mode::any
                                              : eval_mode::scalar);
    }

    // Report whether an evaluated argument iterates the call.
    bool iterates(const value &argument, std::size_t index) const {
        const char letter = letters_[index];
        return pos_.array && argument.is_grid() && letter != 'A' &&
               letter != '?' && letter != 'r' &&
               (argument.grid_cols() > 1 || argument.grid_rows() > 1);
    }

    //
    // Coerce a scalar argument.
    //
    // Returns:
    //      An error to return in place of calling, or nothing.
    //
    std::optional<value> coerce(value &argument, std::size_t index) const {
        const char letter = letters_[index];
        if (letter == 'A' || letter == '?')
            return std::nullopt;
        if (letter == 'r') {
            if (!argument.is_area())
                return value(error_code::value);
            return std::nullopt;
        }
        if (argument.is_grid())
            argument = intersect_scalar(argument, pos_);
        if (letter == 'E')
            return std::nullopt;
        if (argument.is_error())
            return argument;
        if (index >= required_ && argument.is_empty())
            return std::nullopt;
        switch (letter) {
        case 'f': {
            const auto number = argument.to_number();
            if (!number)
                return value(error_code::value);
            argument = value(*number);
            break;
        }
        case 'b': {
            const auto flag = argument.to_bool();
            if (!flag)
                return value(error_code::value);
            argument = value(*flag);
            break;
        }
        case 's':
            if (!argument.is_string())
                argument = value(argument.to_text());
            break;
        default:
            break;
        }
        return std::nullopt;
    }

private:
    const function_def &function_;
    const eval_pos &pos_;
    std::size_t required_ = 0;
    std::string letters_;
};

// A call of a known function.
class call_node final : public node {
public:
    call_node(const function_def *function, std::vector<node_ptr> arguments)
        : function_(function), arguments_(std::move(arguments)) {}

    value eval(const eval_pos &pos, eval_mode mode) const override {
        call_context context{pos, *function_};
        value result = function_->nodes
                           ? function_->nodes(context, arguments_)
                           : call_with_values(context);
        if (mode == eval_mode::scalar && result.is_grid())
            return intersect_scalar(result, pos);
        return result;
    }

    void print(std::string &out, const print_pos &pos) const override {
        out += function_->name;
        print_arguments(out, arguments_, pos);
    }

    void collect(dependency_sink &sink, cell_source *sheet,
                 cell_pos pos) const override {
        if (function_->is_volatile)
            sink.depends_on_time();
        for (const node_ptr &argument : arguments_)
            argument->collect(sink, sheet, pos);
    }

    node_ptr rewrite(const node_ptr &self, ref_mapper &mapper,
                     cell_source *sheet, cell_pos from) const override {
        bool changed = false;
        std::vector<node_ptr> moved;
        moved.reserve(arguments_.size());
        for (const node_ptr &argument : arguments_) {
            moved.push_back(argument->rewrite(argument, mapper, sheet, from));
            changed = changed || moved.back() != argument;
        }
        return changed ? make_call(function_, std::move(moved)) : self;
    }

    const function_def *as_call() const override { return function_; }

    std::vector<node_ptr> children() const override { return arguments_; }

private:
    value call_with_values(call_context &context) const {
        const argument_binder binder(*function_, context.pos);
        if (!binder.accepts(arguments_.size()))
            return value(error_code::value);
        std::vector<value> values;
        values.reserve(arguments_.size());
        int cols = 1;
        int rows = 1;
        bool iterate = false;
        for (std::size_t i = 0; i < arguments_.size(); ++i) {
            values.push_back(binder.evaluate(*arguments_[i], i));
            if (binder.iterates(values.back(), i)) {
                iterate = true;
                cols = std::max(cols, values.back().grid_cols());
                rows = std::max(rows, values.back().grid_rows());
            }
        }
        if (!iterate)
            return one_call(context, binder, values);
        auto result = std::make_shared<array_value>(cols, rows);
        for (int row = 0; row < rows; ++row)
            for (int col = 0; col < cols; ++col) {
                std::vector<value> scalars = values;
                for (std::size_t i = 0; i < scalars.size(); ++i) {
                    if (!binder.iterates(values[i], i))
                        continue;
                    const int w = values[i].grid_cols();
                    const int h = values[i].grid_rows();
                    const int x = w == 1 ? 0 : col;
                    const int y = h == 1 ? 0 : row;
                    scalars[i] = x < w && y < h
                                     ? values[i].grid_at(x, y)
                                     : value(error_code::na);
                }
                result->set(col, row, one_call(context, binder, scalars));
            }
        return value(std::shared_ptr<const array_value>(std::move(result)));
    }

    value one_call(call_context &context, const argument_binder &binder,
                   std::vector<value> &values) const {
        for (std::size_t i = 0; i < values.size(); ++i)
            if (auto failure = binder.coerce(values[i], i))
                return *failure;
        return function_->args(context, values);
    }

    const function_def *function_;
    std::vector<node_ptr> arguments_;
};

// A call of a function this build lacks, kept so saving loses nothing.
class unknown_call_node final : public node {
public:
    unknown_call_node(std::string name, std::vector<node_ptr> arguments)
        : name_(std::move(name)), arguments_(std::move(arguments)) {}

    value eval(const eval_pos &, eval_mode) const override {
        return value(error_code::name);
    }

    void print(std::string &out, const print_pos &pos) const override {
        out += name_;
        print_arguments(out, arguments_, pos);
    }

    void collect(dependency_sink &sink, cell_source *sheet,
                 cell_pos pos) const override {
        for (const node_ptr &argument : arguments_)
            argument->collect(sink, sheet, pos);
    }

    node_ptr rewrite(const node_ptr &self, ref_mapper &mapper,
                     cell_source *sheet, cell_pos from) const override {
        bool changed = false;
        std::vector<node_ptr> moved;
        for (const node_ptr &argument : arguments_) {
            moved.push_back(argument->rewrite(argument, mapper, sheet, from));
            changed = changed || moved.back() != argument;
        }
        return changed ? make_unknown_call(name_, std::move(moved)) : self;
    }

private:
    std::string name_;
    std::vector<node_ptr> arguments_;
};

// A parenthesized list of references.
class set_node final : public node {
public:
    explicit set_node(std::vector<node_ptr> members)
        : members_(std::move(members)) {}

    // A set flattens into a column of its members' values.
    value eval(const eval_pos &pos, eval_mode mode) const override {
        std::vector<value> elements;
        for (const node_ptr &member : members_) {
            const value item = member->eval(pos, eval_mode::any);
            for (int row = 0; row < item.grid_rows(); ++row)
                for (int col = 0; col < item.grid_cols(); ++col)
                    elements.push_back(item.grid_at(col, row));
        }
        const int count = static_cast<int>(elements.size());
        value result(std::make_shared<const array_value>(
            1, count, std::move(elements)));
        return mode == eval_mode::scalar ? intersect_scalar(result, pos)
                                         : result;
    }

    void print(std::string &out, const print_pos &pos) const override {
        print_arguments(out, members_, pos);
    }

    void collect(dependency_sink &sink, cell_source *sheet,
                 cell_pos pos) const override {
        for (const node_ptr &member : members_)
            member->collect(sink, sheet, pos);
    }

    node_ptr rewrite(const node_ptr &self, ref_mapper &mapper,
                     cell_source *sheet, cell_pos from) const override {
        bool changed = false;
        std::vector<node_ptr> moved;
        for (const node_ptr &member : members_) {
            moved.push_back(member->rewrite(member, mapper, sheet, from));
            changed = changed || moved.back() != member;
        }
        return changed ? make_set(std::move(moved)) : self;
    }

private:
    std::vector<node_ptr> members_;
};

// The cells two references share.
class intersection_node final : public node {
public:
    intersection_node(node_ptr left, node_ptr right)
        : left_(std::move(left)), right_(std::move(right)) {}

    value eval(const eval_pos &pos, eval_mode mode) const override {
        const auto cells = as_area(pos);
        if (!cells)
            return value(error_code::null);
        if (mode == eval_mode::any)
            return value(*cells);
        return intersect_scalar(value(*cells), pos);
    }

    void print(std::string &out, const print_pos &pos) const override {
        left_->print(out, pos);
        out.push_back(' ');
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
        return make_intersection(std::move(left), std::move(right));
    }

    std::optional<area> as_area(const eval_pos &pos) const override {
        if (!pos.sheet)
            return area{};
        const auto a = left_->as_area(pos);
        const auto b = right_->as_area(pos);
        if (!a || !b || a->sheet != b->sheet ||
            !a->range.overlaps(b->range))
            return std::nullopt;
        return area{a->sheet, a->range.intersect(b->range)};
    }

private:
    node_ptr left_;
    node_ptr right_;
};

} // namespace

node_ptr make_call(const function_def *function,
                   std::vector<node_ptr> arguments) {
    return std::make_shared<call_node>(function, std::move(arguments));
}

node_ptr make_unknown_call(std::string name, std::vector<node_ptr> arguments) {
    return std::make_shared<unknown_call_node>(std::move(name),
                                               std::move(arguments));
}

node_ptr make_set(std::vector<node_ptr> members) {
    return std::make_shared<set_node>(std::move(members));
}

node_ptr make_intersection(node_ptr left, node_ptr right) {
    return std::make_shared<intersection_node>(std::move(left),
                                               std::move(right));
}

} // namespace expr
