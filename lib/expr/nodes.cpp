//
// The leaf node classes: constants, references and names, each with
// its evaluation, printing, dependencies and rewriting.
//
// Derived from Gnumeric src/expr.c,
// Copyright (C) 2001-2006 Jody Goldberg, 1998-2000 Miguel de Icaza,
// 2000-2018 Morten Welinder.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include <array>
#include <cmath>

#include "expr/function.h"
#include "expr/parser.h"
#include "util/text.h"

namespace expr {
namespace {

using val::area;
using val::array_value;
using val::comparison;
using val::error_code;

// Wrap a coordinate that a relative reference pushed off the sheet.
int wrap(int coordinate, int size) {
    if (coordinate < 0)
        return size + coordinate % size == size
                   ? 0
                   : size + coordinate % size;
    return coordinate >= size ? coordinate % size : coordinate;
}

// Return the sheet a reference reads.
cell_source *target_sheet(const cell_ref &ref, cell_source *formula_sheet) {
    return ref.sheet ? ref.sheet : formula_sheet;
}

// Return a reference's rectangle seen from a position.
cell_range resolve_range(const cell_ref &first, const cell_ref &second,
                         cell_source *sheet, cell_pos from) {
    const int cols = sheet ? sheet->col_count() : val::default_cols;
    const int rows = sheet ? sheet->row_count() : val::default_rows;
    return cell_range::between(first.resolve(from, cols, rows),
                               second.resolve(from, cols, rows));
}

// Return a reference pointing at a position after an edit.
cell_ref reencode(const cell_ref &original, cell_pos target, cell_pos origin) {
    return cell_ref::make(target, origin, original.col_relative,
                          original.row_relative, original.sheet);
}

// Print one corner of a reference.
void print_corner(std::string &out, const cell_ref &ref, cell_pos pos,
                  int cols, int rows) {
    const cell_pos target = ref.resolve(pos, cols, rows);
    if (!ref.col_relative)
        out.push_back('$');
    out += val::col_name(target.col);
    if (!ref.row_relative)
        out.push_back('$');
    out += val::row_name(target.row);
}

// Print a sheet qualifier when a reference names another sheet.
void print_sheet(std::string &out, const cell_ref &ref,
                 const print_pos &pos) {
    if (ref.sheet && ref.sheet != pos.sheet) {
        out += quote_sheet_name(ref.sheet->source_name());
        out.push_back('!');
    }
}

// A literal value.
class constant_node final : public node {
public:
    explicit constant_node(value constant) : constant_(std::move(constant)) {}

    value eval(const eval_pos &pos, eval_mode mode) const override {
        if (mode == eval_mode::scalar && constant_.is_array())
            return intersect_scalar(constant_, pos);
        return constant_;
    }

    void print(std::string &out, const print_pos &) const override {
        if (constant_.is_string()) {
            out.push_back('"');
            for (const char c : constant_.string()) {
                if (c == '"' || c == '\\')
                    out.push_back('\\');
                out.push_back(c);
            }
            out.push_back('"');
        } else if (constant_.is_array()) {
            print_array(out);
        } else if (constant_.is_number()) {
            out += util::shortest_double(constant_.number());
        } else {
            out += constant_.to_text();
        }
    }

    void collect(dependency_sink &, cell_source *, cell_pos) const override {}

    node_ptr rewrite(const node_ptr &self, ref_mapper &, cell_source *,
                     cell_pos) const override {
        return self;
    }

    const value *as_constant() const override { return &constant_; }

private:
    void print_array(std::string &out) const {
        const array_value &elements = constant_.elements();
        const print_pos nowhere{};
        out.push_back('{');
        for (int row = 0; row < elements.rows(); ++row) {
            if (row)
                out.push_back(';');
            for (int col = 0; col < elements.cols(); ++col) {
                if (col)
                    out.push_back(',');
                constant_node(elements.at(col, row)).print(out, nowhere);
            }
        }
        out.push_back('}');
    }

    value constant_;
};

// A reference to one cell.
class cell_node final : public node {
public:
    explicit cell_node(cell_ref ref) : ref_(ref) {}

    value eval(const eval_pos &pos, eval_mode mode) const override {
        cell_source *sheet = target_sheet(ref_, pos.sheet);
        const cell_pos target =
            ref_.resolve(pos.pos, sheet->col_count(), sheet->row_count());
        if (mode == eval_mode::any)
            return value(area{sheet, cell_range::single(target)});
        return sheet->value_at(target);
    }

    void print(std::string &out, const print_pos &pos) const override {
        print_sheet(out, ref_, pos);
        const cell_source *sheet = ref_.sheet ? ref_.sheet : pos.sheet;
        print_corner(out, ref_, pos.pos,
                     sheet ? sheet->col_count() : val::default_cols,
                     sheet ? sheet->row_count() : val::default_rows);
    }

    void collect(dependency_sink &sink, cell_source *sheet,
                 cell_pos pos) const override {
        cell_source *target = target_sheet(ref_, sheet);
        sink.depends_on(target,
                        resolve_range(ref_, ref_, target, pos));
    }

    node_ptr rewrite(const node_ptr &self, ref_mapper &mapper,
                     cell_source *sheet, cell_pos from) const override {
        cell_source *target = target_sheet(ref_, sheet);
        if (mapper.sheet_deleted(target))
            return make_constant(value(error_code::ref));
        const cell_range before = resolve_range(ref_, ref_, target, from);
        const auto after = mapper.map_range(target, before);
        if (!after)
            return make_constant(value(error_code::ref));
        const cell_pos origin = mapper.map_origin(from);
        cell_ref moved = reencode(ref_, after->start, origin);
        return moved == ref_ ? self : make_cell(moved);
    }

    std::optional<area> as_area(const eval_pos &pos) const override {
        cell_source *sheet = target_sheet(ref_, pos.sheet);
        if (!sheet)
            return area{};
        return area{sheet,
                    cell_range::single(ref_.resolve(
                        pos.pos, sheet->col_count(), sheet->row_count()))};
    }

private:
    cell_ref ref_;
};

// A reference to a rectangle.
class range_node final : public node {
public:
    range_node(cell_ref first, cell_ref second)
        : first_(first), second_(second) {}

    value eval(const eval_pos &pos, eval_mode mode) const override {
        cell_source *sheet = target_sheet(first_, pos.sheet);
        const area cells{sheet,
                         resolve_range(first_, second_, sheet, pos.pos)};
        if (mode == eval_mode::any)
            return value(cells);
        return intersect_scalar(value(cells), pos);
    }

    void print(std::string &out, const print_pos &pos) const override {
        print_sheet(out, first_, pos);
        const cell_source *sheet = first_.sheet ? first_.sheet : pos.sheet;
        const int cols = sheet ? sheet->col_count() : val::default_cols;
        const int rows = sheet ? sheet->row_count() : val::default_rows;
        const cell_pos a = first_.resolve(pos.pos, cols, rows);
        const cell_pos b = second_.resolve(pos.pos, cols, rows);
        const bool all_rows = !first_.row_relative && !second_.row_relative &&
                              a.row == 0 && b.row == rows - 1;
        const bool all_cols = !first_.col_relative && !second_.col_relative &&
                              a.col == 0 && b.col == cols - 1;
        if (all_rows && !all_cols) {
            print_axis(out, first_.col_relative, val::col_name(a.col));
            out.push_back(':');
            print_axis(out, second_.col_relative, val::col_name(b.col));
            return;
        }
        if (all_cols && !all_rows) {
            print_axis(out, first_.row_relative, val::row_name(a.row));
            out.push_back(':');
            print_axis(out, second_.row_relative, val::row_name(b.row));
            return;
        }
        print_corner(out, first_, pos.pos, cols, rows);
        out.push_back(':');
        print_corner(out, second_, pos.pos, cols, rows);
    }

    void collect(dependency_sink &sink, cell_source *sheet,
                 cell_pos pos) const override {
        cell_source *target = target_sheet(first_, sheet);
        sink.depends_on(target, resolve_range(first_, second_, target, pos));
    }

    node_ptr rewrite(const node_ptr &self, ref_mapper &mapper,
                     cell_source *sheet, cell_pos from) const override {
        cell_source *target = target_sheet(first_, sheet);
        if (mapper.sheet_deleted(target))
            return make_constant(value(error_code::ref));
        const cell_range before =
            resolve_range(first_, second_, target, from);
        const auto after = mapper.map_range(target, before);
        if (!after)
            return make_constant(value(error_code::ref));
        const cell_pos origin = mapper.map_origin(from);
        cell_ref a = reencode(first_, after->start, origin);
        cell_ref b = reencode(second_, after->end, origin);
        if (a == first_ && b == second_)
            return self;
        return make_range(a, b);
    }

    std::optional<area> as_area(const eval_pos &pos) const override {
        cell_source *sheet = target_sheet(first_, pos.sheet);
        if (!sheet)
            return area{};
        return area{sheet, resolve_range(first_, second_, sheet, pos.pos)};
    }

private:
    static void print_axis(std::string &out, bool relative,
                           const std::string &name) {
        if (!relative)
            out.push_back('$');
        out += name;
    }

    cell_ref first_;
    cell_ref second_;
};

// A reference to a named expression.
class name_node final : public node {
public:
    explicit name_node(std::shared_ptr<named_expr> name)
        : name_(std::move(name)) {}

    value eval(const eval_pos &pos, eval_mode mode) const override {
        if (!name_->definition)
            return value(error_code::name);
        // Relative parts of a name are relative to where it is used.
        eval_pos inner = pos;
        return name_->definition->eval(inner, mode);
    }

    void print(std::string &out, const print_pos &pos) const override {
        if (name_->sheet && name_->sheet != pos.sheet) {
            out += quote_sheet_name(name_->sheet->source_name());
            out.push_back('!');
        }
        out += name_->name;
    }

    void collect(dependency_sink &sink, cell_source *sheet,
                 cell_pos pos) const override {
        if (name_->definition)
            name_->definition->collect(sink, sheet, pos);
    }

    node_ptr rewrite(const node_ptr &self, ref_mapper &, cell_source *,
                     cell_pos) const override {
        return self;
    }

    std::optional<area> as_area(const eval_pos &pos) const override {
        if (!pos.sheet)
            return std::nullopt;
        return name_->definition ? name_->definition->as_area(pos)
                                 : std::nullopt;
    }

private:
    std::shared_ptr<named_expr> name_;
};

} // namespace

cell_pos cell_ref::resolve(cell_pos from, int cols, int rows) const {
    return {col_relative ? wrap(from.col + col, cols) : col,
            row_relative ? wrap(from.row + row, rows) : row};
}

cell_ref cell_ref::make(cell_pos target, cell_pos from, bool col_relative,
                        bool row_relative, cell_source *sheet) {
    cell_ref ref;
    ref.sheet = sheet;
    ref.col_relative = col_relative;
    ref.row_relative = row_relative;
    ref.col = col_relative ? target.col - from.col : target.col;
    ref.row = row_relative ? target.row - from.row : target.row;
    return ref;
}

node_ptr make_constant(value constant) {
    return std::make_shared<constant_node>(std::move(constant));
}

node_ptr make_cell(cell_ref ref) {
    return std::make_shared<cell_node>(ref);
}

node_ptr make_range(cell_ref first, cell_ref second) {
    return std::make_shared<range_node>(first, second);
}

node_ptr make_name(std::shared_ptr<named_expr> name) {
    return std::make_shared<name_node>(std::move(name));
}

std::string to_text(const node &tree, const print_pos &pos) {
    std::string out;
    tree.print(out, pos);
    return out;
}

value intersect_scalar(const value &grid, const eval_pos &pos) {
    if (grid.is_array())
        return grid.elements().at(0, 0);
    if (!grid.is_area())
        return grid;
    const val::area &cells = grid.cells();
    const cell_range &range = cells.range;
    if (range.is_single())
        return cells.sheet->value_at(range.start);
    if (range.cols() == 1 && pos.pos.row >= range.start.row &&
        pos.pos.row <= range.end.row)
        return cells.sheet->value_at({range.start.col, pos.pos.row});
    if (range.rows() == 1 && pos.pos.col >= range.start.col &&
        pos.pos.col <= range.end.col)
        return cells.sheet->value_at({pos.pos.col, range.start.row});
    if (pos.array)
        return cells.sheet->value_at(range.start);
    return value(error_code::value);
}

} // namespace expr
