//
// The command classes. region_edit is the workhorse: it remembers a
// rectangle, runs an operation, and restores the rectangle on undo.
//
// Derived from Gnumeric src/commands.c,
// Copyright (C) 1999-2008 Jody Goldberg, 2002-2009 Morten Welinder.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include "cmd/edits.h"

#include <functional>

#include "numfmt/format.h"

namespace cmd {
namespace {

using book::region;
using book::rewrite_record;
using book::workbook;

// Restore a remembered rectangle exactly: contents, styles and merges.
void restore(sheet &where, const cell_range &range, const region &saved) {
    where.clear_range(range);
    where.unmerge(range);
    book::paste_region(where, range.start, saved);
}

// A sequence of rectangles around one operation.
class region_edit final : public command {
public:
    using action = std::function<void(sheet &)>;

    region_edit(sheet &where, std::vector<cell_range> ranges, action apply,
                std::string name)
        : where_(where.name()), ranges_(std::move(ranges)),
          apply_(std::move(apply)), name_(std::move(name)) {}

    void redo(workbook &target) override {
        sheet &item = find(target);
        saved_.clear();
        for (const cell_range &range : ranges_)
            saved_.push_back(book::copy_region(item, range));
        apply_(item);
    }

    void undo(workbook &target) override {
        sheet &item = find(target);
        for (std::size_t i = ranges_.size(); i-- > 0;)
            restore(item, ranges_[i], saved_[i]);
    }

    std::string describe() const override { return name_; }

private:
    sheet &find(workbook &target) const {
        return static_cast<sheet &>(*target.find_sheet(where_));
    }

    std::string where_;
    std::vector<cell_range> ranges_;
    action apply_;
    std::string name_;
    std::vector<region> saved_;
};

// Make a region edit.
std::unique_ptr<command> edit(sheet &where, std::vector<cell_range> ranges,
                              region_edit::action apply, std::string name) {
    return std::make_unique<region_edit>(where, std::move(ranges),
                                         std::move(apply), std::move(name));
}

// Inserts or deletes lines of an axis.
class line_edit final : public command {
public:
    line_edit(sheet &where, bool rows, int at, int count, bool insert)
        : where_(where.name()), rows_(rows), at_(at), count_(count),
          insert_(insert) {
        const int size = rows ? where.row_count() : where.col_count();
        // Cells pushed off the end by an insert, or removed by a delete.
        const cell_pos last{where.col_count() - 1, where.row_count() - 1};
        if (insert_)
            lost_ = rows ? cell_range{{0, size - count}, last}
                         : cell_range{{size - count, 0}, last};
        else
            lost_ = rows ? cell_range{{0, at}, {last.col, at + count - 1}}
                         : cell_range{{at, 0}, {at + count - 1, last.row}};
    }

    void redo(workbook &target) override {
        sheet &item = find(target);
        saved_ = book::copy_region(item, lost_);
        if (rows_ && !insert_)
            for (int i = 0; i < count_; ++i)
                sizes_.push_back(item.rows().points(at_ + i));
        if (!rows_ && !insert_)
            for (int i = 0; i < count_; ++i)
                sizes_.push_back(item.cols().points(at_ + i));
        record_ = apply(target, item, insert_);
    }

    void undo(workbook &target) override {
        sheet &item = find(target);
        apply(target, item, !insert_);
        restore(item, lost_, saved_);
        target.restore_formulas(record_);
        for (std::size_t i = 0; i < sizes_.size(); ++i) {
            book::axis &axis = rows_ ? item.rows() : item.cols();
            if (sizes_[i] != axis.default_points())
                axis.set_points(at_ + static_cast<int>(i), sizes_[i], true);
        }
        sizes_.clear();
        target.rebuild_dependencies();
        target.queue_all();
    }

    std::string describe() const override {
        const char *noun = rows_ ? "rows" : "columns";
        return std::string(insert_ ? "Insert " : "Delete ") + noun;
    }

private:
    rewrite_record apply(workbook &target, sheet &item, bool insert) {
        if (rows_)
            return insert ? target.insert_rows(item, at_, count_)
                          : target.delete_rows(item, at_, count_);
        return insert ? target.insert_cols(item, at_, count_)
                      : target.delete_cols(item, at_, count_);
    }

    sheet &find(workbook &target) const {
        return static_cast<sheet &>(*target.find_sheet(where_));
    }

    std::string where_;
    bool rows_;
    int at_;
    int count_;
    bool insert_;
    cell_range lost_;
    region saved_;
    rewrite_record record_;
    std::vector<double> sizes_;
};

// Moves a rectangle, references following.
class move_edit final : public command {
public:
    move_edit(sheet &from, const cell_range &source, sheet &to, cell_pos corner)
        : from_(from.name()), to_(to.name()), source_(source),
          target_{corner, {corner.col + source.cols() - 1,
                           corner.row + source.rows() - 1}} {}

    void redo(workbook &book) override {
        sheet &from = static_cast<sheet &>(*book.find_sheet(from_));
        sheet &to = static_cast<sheet &>(*book.find_sheet(to_));
        saved_source_ = book::copy_region(from, source_);
        saved_target_ = book::copy_region(to, target_);
        if (&from == &to) {
            record_ = book.move_range(from, source_, to, target_.start);
            const auto base = book.styles().default_style();
            from.styles().set(source_, base);
            for (const auto &run : saved_source_.styles)
                to.styles().set(
                    {{target_.start.col + run.range.start.col,
                      target_.start.row + run.range.start.row},
                     {target_.start.col + run.range.end.col,
                      target_.start.row + run.range.end.row}},
                    run.item);
        } else {
            book::paste_region(to, target_.start, saved_source_);
            from.clear_range(source_);
            from.styles().set(source_, book.styles().default_style());
        }
        book.queue_all();
    }

    void undo(workbook &book) override {
        sheet &from = static_cast<sheet &>(*book.find_sheet(from_));
        sheet &to = static_cast<sheet &>(*book.find_sheet(to_));
        to.clear_range(target_);
        restore(to, target_, saved_target_);
        restore(from, source_, saved_source_);
        book.restore_formulas(record_);
        book.rebuild_dependencies();
        book.queue_all();
    }

    std::string describe() const override { return "Move cells"; }

private:
    std::string from_;
    std::string to_;
    cell_range source_;
    cell_range target_;
    region saved_source_;
    region saved_target_;
    rewrite_record record_;
};

// Changes sizes or visibility on an axis.
class axis_edit final : public command {
public:
    using action = std::function<void(book::axis &, int)>;

    axis_edit(sheet &where, bool cols, int first, int last, action apply,
              std::string name)
        : where_(where.name()), cols_(cols), first_(first), last_(last),
          apply_(std::move(apply)), name_(std::move(name)) {}

    void redo(workbook &target) override {
        book::axis &axis = find(target);
        saved_.clear();
        for (int i = first_; i <= last_; ++i) {
            const auto entry = axis.entries().find(i);
            saved_.push_back(entry == axis.entries().end()
                                 ? std::nullopt
                                 : std::make_optional(entry->second));
            apply_(axis, i);
        }
    }

    void undo(workbook &target) override {
        book::axis &axis = find(target);
        for (int i = first_; i <= last_; ++i) {
            const auto &entry = saved_[static_cast<std::size_t>(i - first_)];
            axis.reset(i);
            if (entry) {
                axis.set_points(i, entry->points, entry->hard);
                axis.set_hidden(i, entry->hidden);
            }
        }
    }

    std::string describe() const override { return name_; }

private:
    book::axis &find(workbook &target) const {
        sheet &item = static_cast<sheet &>(*target.find_sheet(where_));
        return cols_ ? item.cols() : item.rows();
    }

    std::string where_;
    bool cols_;
    int first_;
    int last_;
    action apply_;
    std::string name_;
    std::vector<std::optional<book::axis_entry>> saved_;
};

// Adds or removes a sheet. A taken sheet stays alive here, so formulas
// restored by undo point at the sheet they always did.
class sheet_edit final : public command {
public:
    sheet_edit(int index, std::string name, bool add)
        : index_(index), name_(std::move(name)), add_(add) {}

    void redo(workbook &target) override {
        if (add_)
            create(target);
        else
            held_ = target.take_sheet(index_);
    }

    void undo(workbook &target) override {
        if (add_)
            held_ = target.take_sheet(index_);
        else
            create(target);
    }

    std::string describe() const override {
        return add_ ? "Add sheet" : "Delete sheet";
    }

private:
    void create(workbook &target) {
        if (held_.item) {
            target.restore_sheet(index_, std::move(held_));
            held_ = {};
        } else {
            target.insert_sheet(index_, name_);
        }
    }

    int index_;
    std::string name_;
    bool add_;
    book::removed_sheet held_;
};

// Renames a sheet.
class rename_edit final : public command {
public:
    rename_edit(int index, std::string name)
        : index_(index), name_(std::move(name)) {}

    void redo(workbook &target) override { swap(target); }
    void undo(workbook &target) override { swap(target); }
    std::string describe() const override { return "Rename sheet"; }

private:
    void swap(workbook &target) {
        sheet &item = target.sheet_at(index_);
        std::string previous = item.name();
        item.set_name(std::move(name_));
        name_ = std::move(previous);
    }

    int index_;
    std::string name_;
};

// Moves a sheet in the order.
class order_edit final : public command {
public:
    order_edit(int from, int to) : from_(from), to_(to) {}

    void redo(workbook &target) override { target.move_sheet(from_, to_); }
    void undo(workbook &target) override { target.move_sheet(to_, from_); }
    std::string describe() const override { return "Move sheet"; }

private:
    int from_;
    int to_;
};

// Defines a name, or removes one.
class name_edit final : public command {
public:
    name_edit(std::string name, expr::node_ptr formula, sheet *scope,
              cell_pos origin)
        : name_(std::move(name)), formula_(std::move(formula)), scope_(scope),
          origin_(origin) {}

    void redo(workbook &target) override { swap(target); }
    void undo(workbook &target) override { swap(target); }
    std::string describe() const override { return "Define name"; }

private:
    void swap(workbook &target) {
        auto existing = target.find_name(name_, scope_, false);
        expr::node_ptr previous = existing ? existing->definition : nullptr;
        if (formula_)
            target.define_name(name_, formula_, scope_, origin_);
        else if (existing)
            target.undefine_name(existing);
        formula_ = std::move(previous);
    }

    std::string name_;
    expr::node_ptr formula_;
    sheet *scope_;
    cell_pos origin_;
};

// Return every used cell of a sheet as a range, or the first cell.
cell_range used_or_first(sheet &where) {
    return where.used_range().value_or(cell_range{});
}

} // namespace

std::unique_ptr<command> set_entry(sheet &where, cell_pos pos,
                                   std::string text) {
    return edit(where, {cell_range::single(pos)},
                [pos, text = std::move(text)](sheet &item) {
                    book::store_entry(item, pos, text);
                },
                "Set cell");
}

std::unique_ptr<command> fill_entry(sheet &where, const cell_range &range,
                                    std::string text) {
    return edit(where, {range},
                [range, text = std::move(text)](sheet &item) {
                    const cell_pos first = range.start;
                    book::store_entry(item, first, text);
                    const book::region lifted =
                        book::copy_region(item, cell_range::single(first));
                    for (int row = range.start.row; row <= range.end.row; ++row)
                        for (int col = range.start.col; col <= range.end.col;
                             ++col)
                            if (cell_pos{col, row} != first)
                                book::paste_region(item, {col, row}, lifted);
                },
                "Fill cells");
}

std::unique_ptr<command> set_array(sheet &where, const cell_range &range,
                                   expr::node_ptr formula) {
    return edit(where, {range},
                [range, formula = std::move(formula)](sheet &item) {
                    item.set_array(range, formula);
                },
                "Enter array formula");
}

std::unique_ptr<command> clear_cells(sheet &where,
                                     std::vector<cell_range> ranges,
                                     clear_kind kind) {
    const std::vector<cell_range> targets = ranges;
    return edit(where, std::move(ranges),
                [targets, kind](sheet &item) {
                    for (const cell_range &range : targets) {
                        if (kind != clear_kind::formats)
                            item.clear_range(range);
                        if (kind != clear_kind::contents)
                            item.styles().set(
                                range, item.book().styles().default_style());
                    }
                },
                "Clear");
}

std::unique_ptr<command> format_cells(sheet &where,
                                      std::vector<cell_range> ranges,
                                      book::style_patch patch,
                                      std::string name) {
    const std::vector<cell_range> targets = ranges;
    return edit(where, std::move(ranges),
                [targets, patch = std::move(patch)](sheet &item) {
                    for (const cell_range &range : targets)
                        item.apply_style(range, patch);
                },
                std::move(name));
}

std::unique_ptr<command> paste(sheet &where, const cell_range &target,
                               book::region item, book::paste_options options) {
    const int cols = options.transpose ? item.rows : item.cols;
    const int rows = options.transpose ? item.cols : item.rows;
    const bool repeat = target.cols() > cols || target.rows() > rows;
    cell_range saved = target;
    if (!repeat)
        saved.end = {std::min(target.start.col + cols, where.col_count()) - 1,
                     std::min(target.start.row + rows, where.row_count()) - 1};
    return edit(where, {saved},
                [=, item = std::move(item)](sheet &into) {
                    if (repeat)
                        book::paste_repeated(into, target, item, options);
                    else
                        book::paste_region(into, target.start, item, options);
                },
                "Paste");
}

std::unique_ptr<command> paste_text(sheet &where, cell_pos corner,
                                    std::string text) {
    int rows = 1;
    int cols = 1;
    int line_cols = 1;
    for (const char c : text) {
        if (c == '\t') {
            cols = std::max(cols, ++line_cols);
        } else if (c == '\n') {
            ++rows;
            line_cols = 1;
        }
    }
    const cell_range saved{
        corner,
        {std::min(corner.col + cols, where.col_count()) - 1,
         std::min(corner.row + rows, where.row_count()) - 1}};
    return edit(where, {saved},
                [corner, text = std::move(text)](sheet &into) {
                    book::paste_text(into, corner, text);
                },
                "Paste");
}

std::unique_ptr<command> move_range(sheet &from, const cell_range &source,
                                    sheet &to, cell_pos corner) {
    return std::make_unique<move_edit>(from, source, to, corner);
}

std::unique_ptr<command> autofill(sheet &where, const cell_range &source,
                                  const cell_range &target) {
    return edit(where, {target},
                [source, target](sheet &item) {
                    book::autofill(item, source, target);
                },
                "Autofill");
}

std::unique_ptr<command> sort(sheet &where, const cell_range &range,
                              std::vector<book::sort_key> keys, bool by_rows) {
    return edit(where, {range},
                [=, keys = std::move(keys)](sheet &item) {
                    if (by_rows)
                        book::sort_rows(item, range, keys);
                    else
                        book::sort_cols(item, range, keys);
                },
                "Sort");
}

std::unique_ptr<command> change_rows(sheet &where, int at, int count,
                                     bool insert) {
    return std::make_unique<line_edit>(where, true, at, count, insert);
}

std::unique_ptr<command> change_cols(sheet &where, int at, int count,
                                     bool insert) {
    return std::make_unique<line_edit>(where, false, at, count, insert);
}

std::unique_ptr<command> resize(sheet &where, bool cols, int first, int last,
                                double points) {
    return std::make_unique<axis_edit>(
        where, cols, first, last,
        [points](book::axis &axis, int index) {
            axis.set_points(index, points, true);
        },
        cols ? "Set column width" : "Set row height");
}

std::unique_ptr<command> set_hidden(sheet &where, bool cols, int first,
                                    int last, bool hidden) {
    return std::make_unique<axis_edit>(
        where, cols, first, last,
        [hidden](book::axis &axis, int index) {
            axis.set_hidden(index, hidden);
        },
        hidden ? "Hide" : "Unhide");
}

std::unique_ptr<command> merge_cells(sheet &where, const cell_range &range,
                                     bool merge) {
    return edit(where, {range},
                [range, merge](sheet &item) {
                    if (merge)
                        item.merge(range);
                    else
                        item.unmerge(range);
                },
                merge ? "Merge cells" : "Unmerge cells");
}

std::unique_ptr<command> add_sheet(int index, std::string name) {
    return std::make_unique<sheet_edit>(index, std::move(name), true);
}

std::unique_ptr<command> remove_sheet(int index) {
    return std::make_unique<sheet_edit>(index, std::string(), false);
}

std::unique_ptr<command> rename_sheet(int index, std::string name) {
    return std::make_unique<rename_edit>(index, std::move(name));
}

std::unique_ptr<command> move_sheet(int from, int to) {
    return std::make_unique<order_edit>(from, to);
}

std::unique_ptr<command> define_name(std::string name, expr::node_ptr formula,
                                     sheet *scope, cell_pos origin) {
    return std::make_unique<name_edit>(std::move(name), std::move(formula),
                                       scope, origin);
}

std::unique_ptr<command> replace_all(sheet &where, std::string text,
                                     std::string replacement,
                                     book::search_options options) {
    return edit(where, {used_or_first(where)},
                [text = std::move(text), replacement = std::move(replacement),
                 options](sheet &item) {
                    std::vector<cell_pos> positions;
                    item.for_each_stored([&](book::cell &stored) {
                        positions.push_back(stored.pos());
                    });
                    for (const cell_pos pos : positions)
                        book::replace_in_cell(item, pos, text, replacement,
                                              options);
                },
                "Replace");
}

} // namespace cmd
