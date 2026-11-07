//
// Structural edits: inserting and deleting rows and columns, and moving
// a rectangle. Each builds a mapper, rewrites formulas through it, moves
// storage, and rebuilds the graph. Rebuilding is linear in formulas, but
// these edits are rare and the code has no incremental case to get wrong.
//
// Derived from Gnumeric src/sheet.c (sheet_insert_rows and friends) and
// src/expr.c (gnm_expr_relocate),
// Copyright (C) 2000-2007 Jody Goldberg, 2000-2018 Morten Welinder.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include <algorithm>

#include "book/workbook.h"

namespace book {
namespace {

// Maps references across an insert or delete on one axis.
class axis_shift final : public expr::ref_mapper {
public:
    axis_shift(sheet &where, bool rows, int at, int count)
        : where_(&where), rows_(rows), at_(at), count_(count) {}

    std::optional<cell_range> map_range(val::cell_source *target,
                                        cell_range range) override {
        if (target != where_)
            return range;
        int first = rows_ ? range.start.row : range.start.col;
        int last = rows_ ? range.end.row : range.end.col;
        const int size = rows_ ? where_->row_count() : where_->col_count();
        // A whole axis stays whole.
        if (first == 0 && last == size - 1)
            return range;
        if (count_ > 0) {
            if (first >= at_)
                first += count_;
            if (last >= at_)
                last += count_;
            if (first >= size)
                return std::nullopt;
            last = std::min(last, size - 1);
        } else {
            const int end = at_ - count_;
            if (first >= at_ && last < end)
                return std::nullopt;
            if (first >= end)
                first += count_;
            else if (first > at_)
                first = at_;
            if (last >= end)
                last += count_;
            else if (last >= at_)
                last = at_ - 1;
        }
        cell_range result = range;
        if (rows_) {
            result.start.row = first;
            result.end.row = last;
        } else {
            result.start.col = first;
            result.end.col = last;
        }
        return result;
    }

    cell_pos map_origin(cell_pos pos) override {
        if (origin_sheet_ != where_)
            return pos;
        int &place = rows_ ? pos.row : pos.col;
        if (count_ > 0) {
            if (place >= at_)
                place += count_;
        } else if (place >= at_ - count_) {
            place += count_;
        }
        return pos;
    }

    // Name the sheet of the formula about to be mapped.
    void set_origin_sheet(sheet *item) { origin_sheet_ = item; }

private:
    sheet *where_;
    sheet *origin_sheet_ = nullptr;
    bool rows_;
    int at_;
    int count_;
};

// Walks every formula with its sheet known to the mapper.
rewrite_record rewrite_all(workbook &book, axis_shift &mapper) {
    rewrite_record total;
    for (int i = 0; i < book.sheet_count(); ++i) {
        sheet &owner = book.sheet_at(i);
        mapper.set_origin_sheet(&owner);
        std::vector<cell *> formulas;
        owner.for_each_formula(
            [&](cell &formula) { formulas.push_back(&formula); });
        for (cell *formula : formulas) {
            expr::node_ptr moved = formula->formula->rewrite(
                formula->formula, mapper, &owner, formula->pos());
            if (moved == formula->formula)
                continue;
            total.formulas.push_back({&owner, formula->pos(), formula->formula});
            formula->formula = std::move(moved);
        }
    }
    mapper.set_origin_sheet(nullptr);
    for (const auto &name : book.names()) {
        if (!name->definition)
            continue;
        expr::node_ptr moved = name->definition->rewrite(
            name->definition, mapper, name->sheet, name->origin);
        if (moved != name->definition) {
            total.names.push_back({name, name->definition});
            name->definition = std::move(moved);
        }
    }
    return total;
}

// Maps references to a moved rectangle.
class range_move final : public expr::ref_mapper {
public:
    range_move(sheet &from, const cell_range &source, sheet &to, cell_pos corner)
        : from_(&from), source_(source), to_(&to),
          delta_{corner.col - source.start.col, corner.row - source.start.row} {
    }

    std::optional<cell_range> map_range(val::cell_source *target,
                                        cell_range range) override {
        if (target != from_ || !source_.contains(range))
            return range;
        moved_sheet_ = true;
        return cell_range{translate(range.start), translate(range.end)};
    }

    cell_pos map_origin(cell_pos pos) override {
        return origin_moves_ && source_.contains(pos) ? translate(pos) : pos;
    }

    // Name the sheet of the formula about to be mapped.
    void set_origin_sheet(sheet *item) { origin_moves_ = item == from_; }

private:
    cell_pos translate(cell_pos pos) const {
        return {pos.col + delta_.col, pos.row + delta_.row};
    }

    sheet *from_;
    cell_range source_;
    sheet *to_;
    cell_pos delta_;
    bool moved_sheet_ = false;
    bool origin_moves_ = false;
};

// Rewrites all formulas for a moved rectangle.
rewrite_record rewrite_move(workbook &book, range_move &mapper) {
    rewrite_record total;
    for (int i = 0; i < book.sheet_count(); ++i) {
        sheet &owner = book.sheet_at(i);
        mapper.set_origin_sheet(&owner);
        std::vector<cell *> formulas;
        owner.for_each_formula(
            [&](cell &formula) { formulas.push_back(&formula); });
        for (cell *formula : formulas) {
            expr::node_ptr moved = formula->formula->rewrite(
                formula->formula, mapper, &owner, formula->pos());
            if (moved == formula->formula)
                continue;
            total.formulas.push_back({&owner, formula->pos(), formula->formula});
            formula->formula = std::move(moved);
        }
    }
    return total;
}

} // namespace

rewrite_record workbook::insert_rows(sheet &where, int at, int count) {
    axis_shift mapper(where, true, at, count);
    rewrite_record record = rewrite_all(*this, mapper);
    const int rows = where.row_count();
    where.remap_cells([&](cell_pos pos) -> std::optional<cell_pos> {
        if (pos.row < at)
            return pos;
        if (pos.row + count >= rows)
            return std::nullopt;
        return cell_pos{pos.col, pos.row + count};
    });
    where.shift_rows_storage(at, count);
    rebuild_dependencies();
    queue_all();
    return record;
}

rewrite_record workbook::delete_rows(sheet &where, int at, int count) {
    axis_shift mapper(where, true, at, -count);
    rewrite_record record = rewrite_all(*this, mapper);
    where.remap_cells([&](cell_pos pos) -> std::optional<cell_pos> {
        if (pos.row < at)
            return pos;
        if (pos.row < at + count)
            return std::nullopt;
        return cell_pos{pos.col, pos.row - count};
    });
    where.shift_rows_storage(at, -count);
    rebuild_dependencies();
    queue_all();
    return record;
}

rewrite_record workbook::insert_cols(sheet &where, int at, int count) {
    axis_shift mapper(where, false, at, count);
    rewrite_record record = rewrite_all(*this, mapper);
    const int cols = where.col_count();
    where.remap_cells([&](cell_pos pos) -> std::optional<cell_pos> {
        if (pos.col < at)
            return pos;
        if (pos.col + count >= cols)
            return std::nullopt;
        return cell_pos{pos.col + count, pos.row};
    });
    where.shift_cols_storage(at, count);
    rebuild_dependencies();
    queue_all();
    return record;
}

rewrite_record workbook::delete_cols(sheet &where, int at, int count) {
    axis_shift mapper(where, false, at, -count);
    rewrite_record record = rewrite_all(*this, mapper);
    where.remap_cells([&](cell_pos pos) -> std::optional<cell_pos> {
        if (pos.col < at)
            return pos;
        if (pos.col < at + count)
            return std::nullopt;
        return cell_pos{pos.col - count, pos.row};
    });
    where.shift_cols_storage(at, -count);
    rebuild_dependencies();
    queue_all();
    return record;
}

rewrite_record workbook::move_range(sheet &where, const cell_range &source,
                                    sheet &target, cell_pos corner) {
    range_move mapper(where, source, target, corner);
    rewrite_record record = rewrite_move(*this, mapper);
    const cell_pos delta{corner.col - source.start.col,
                         corner.row - source.start.row};
    if (&where == &target) {
        const cell_range landing{corner,
                                 {corner.col + source.cols() - 1,
                                  corner.row + source.rows() - 1}};
        where.remap_cells([&](cell_pos pos) -> std::optional<cell_pos> {
            if (source.contains(pos))
                return cell_pos{pos.col + delta.col, pos.row + delta.row};
            if (landing.contains(pos))
                return std::nullopt;
            return pos;
        });
    }
    rebuild_dependencies();
    queue_all();
    return record;
}

} // namespace book
