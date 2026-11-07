//
// Cell storage, entries and shifting.
//
// Derived from Gnumeric src/sheet.c and src/cell.c,
// Copyright (C) 2000-2007 Jody Goldberg, 1998-2001 Miguel de Icaza,
// Copyright (C) 2000-2018 Morten Welinder.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include "book/sheet.h"

#include <algorithm>
#include <cmath>

#include "book/workbook.h"
#include "expr/parser.h"
#include "numfmt/date.h"
#include "numfmt/format.h"
#include "numfmt/match.h"
#include "util/text.h"

namespace book {
namespace {

// Gnumeric's default sizes in points.
constexpr double default_col_points = 48;
constexpr double default_row_points = 12.75;

// The value of a cell that does not exist.
const value no_value;

// Return a format suggested for a formula's result.
std::string format_for(expr::auto_format kind) {
    switch (kind) {
    case expr::auto_format::date:
        return "m/d/yyyy";
    case expr::auto_format::time:
        return "h:mm";
    case expr::auto_format::percent:
        return "0.00%";
    case expr::auto_format::money:
        return "$#,##0.00";
    default:
        return {};
    }
}

} // namespace

sheet::sheet(workbook &book, std::string name, int cols, int rows)
    : book_(&book), name_(std::move(name)), cols_(cols), rows_(rows),
      styles_(cols, rows, book.styles().default_style()),
      col_axis_(cols, default_col_points),
      row_axis_(rows, default_row_points) {}

sheet::~sheet() {
    for (auto &[col, cells] : columns_)
        for (auto &[row, item] : cells)
            if (item.formula) {
                book_->detach(item);
                book_->volatile_.erase(&item);
            }
}

cell *sheet::find(cell_pos pos) {
    const auto column_entry = columns_.find(pos.col);
    if (column_entry == columns_.end())
        return nullptr;
    const auto found = column_entry->second.find(pos.row);
    return found == column_entry->second.end() ? nullptr : &found->second;
}

const cell *sheet::find(cell_pos pos) const {
    return const_cast<sheet *>(this)->find(pos);
}

cell &sheet::fetch(cell_pos pos) {
    auto &cells = columns_[pos.col];
    const auto found = cells.find(pos.row);
    if (found != cells.end())
        return found->second;
    return cells.try_emplace(pos.row, *this, pos).first->second;
}

const value &sheet::value_at(cell_pos pos) {
    cell *item = find(pos);
    if (!item)
        return no_value;
    if (item->dirty && !item->being_calculated)
        book_->calculate(*item);
    return item->result;
}

void sheet::for_each_cell(
    const cell_range &range,
    const std::function<void(cell_pos, const value &)> &visit) {
    for (auto column_entry = columns_.lower_bound(range.start.col);
         column_entry != columns_.end() &&
         column_entry->first <= range.end.col;
         ++column_entry) {
        auto &cells = column_entry->second;
        for (auto entry = cells.lower_bound(range.start.row);
             entry != cells.end() && entry->first <= range.end.row;
             ++entry) {
            cell &item = entry->second;
            if (item.dirty && !item.being_calculated)
                book_->calculate(item);
            visit(item.pos(), item.result);
        }
    }
}

bool sheet::is_formula(cell_pos pos) {
    const cell *item = find(pos);
    return item && item->has_formula();
}

std::optional<std::string> sheet::formula_text(cell_pos pos) {
    const cell *item = find(pos);
    if (!item)
        return std::nullopt;
    const cell *corner = item->in_array ? find(item->array.corner) : item;
    if (!corner || !corner->formula)
        return std::nullopt;
    return "=" + expr::to_text(*corner->formula,
                               {this, corner->pos()});
}

void sheet::forget_formula(cell &item) {
    if (item.formula) {
        book_->detach(item);
        book_->volatile_.erase(&item);
        item.formula.reset();
    }
    if (item.in_array && !item.reads.empty())
        book_->dependents_.remove(item);
    if (item.dirty)
        set_dirty_mark(item.pos(), false);
    item.dirty = false;
    item.reads.clear();
    item.is_volatile = false;
    item.in_array = false;
    item.array = {};
}

void sheet::content_changed(cell &item) {
    if (item.formula) {
        book_->attach(item);
        item.dirty = true;
        set_dirty_mark(item.pos(), true);
    }
    book_->queue_readers(*this, item.pos());
}

void sheet::set_constant(cell_pos pos, value constant,
                         std::string value_format) {
    cell &item = fetch(pos);
    forget_formula(item);
    item.result = std::move(constant);
    item.value_format = std::move(value_format);
    content_changed(item);
}

void sheet::set_formula(cell_pos pos, expr::node_ptr formula) {
    cell &item = fetch(pos);
    forget_formula(item);
    item.formula = std::move(formula);
    item.result = value();
    item.value_format.clear();
    content_changed(item);
}

void sheet::set_array(const cell_range &range, expr::node_ptr formula) {
    cell &corner = fetch(range.start);
    forget_formula(corner);
    corner.formula = std::move(formula);
    corner.result = value();
    corner.array = {range.start, range.cols(), range.rows()};
    corner.in_array = true;
    for (int row = range.start.row; row <= range.end.row; ++row)
        for (int col = range.start.col; col <= range.end.col; ++col) {
            if (cell_pos{col, row} == range.start)
                continue;
            cell &element = fetch({col, row});
            forget_formula(element);
            element.result = value();
            element.in_array = true;
            element.array = corner.array;
            element.reads = {{this, cell_range::single(range.start)}};
            book_->dependents_.add(element);
        }
    content_changed(corner);
}

void sheet::clear_cell(cell_pos pos) {
    cell *item = find(pos);
    if (!item)
        return;
    forget_formula(*item);
    item->result = value();
    book_->queue_readers(*this, pos);
    auto &cells = columns_[pos.col];
    cells.erase(pos.row);
    if (cells.empty())
        columns_.erase(pos.col);
}

void sheet::clear_range(const cell_range &range) {
    std::vector<cell_pos> present;
    for (auto column_entry = columns_.lower_bound(range.start.col);
         column_entry != columns_.end() &&
         column_entry->first <= range.end.col;
         ++column_entry)
        for (auto entry = column_entry->second.lower_bound(range.start.row);
             entry != column_entry->second.end() &&
             entry->first <= range.end.row;
             ++entry)
            present.push_back(entry->second.pos());
    for (const cell_pos pos : present)
        clear_cell(pos);
}

parsed_entry sheet::parse_entry(cell_pos pos, std::string_view text) {
    parsed_entry entry;
    if (text.empty())
        return entry;
    if (text[0] == '\'') {
        entry.constant = value(text.substr(1));
        return entry;
    }
    const auto format = numfmt::number_format::get(style_at(pos)->format);
    const expr::parse_pos where{this, pos, book_};
    const bool formula_start =
        text.size() > 1 && (text[0] == '=' || text[0] == '+' ||
                            text[0] == '-');
    if (format->family() != numfmt::format_family::text) {
        if (text[0] != '=') {
            if (const auto number = numfmt::match_number(text)) {
                entry.constant = value(number->value);
                entry.format = number->format;
                return entry;
            }
            if (const auto flag = val::parse_boolean(text)) {
                entry.constant = value(*flag);
                return entry;
            }
            if (const auto code = val::parse_error(text)) {
                entry.constant = value(*code);
                return entry;
            }
        }
        if (formula_start) {
            const std::string_view body =
                text[0] == '=' ? text.substr(1) : text;
            expr::parse_result parsed = expr::parse_formula(body, where);
            if (parsed.tree) {
                entry.formula = std::move(parsed.tree);
                entry.format =
                    format_for(expr::suggest_format(*entry.formula));
                return entry;
            }
            if (text[0] == '=') {
                entry.error = parsed.error;
                entry.constant = value(text);
                return entry;
            }
        }
    }
    entry.constant = value(text);
    return entry;
}

std::string sheet::entry_text(cell_pos pos) {
    if (const auto formula = formula_text(pos))
        return *formula;
    const cell *item = find(pos);
    if (!item)
        return {};
    const value &stored = item->result;
    if (stored.is_string()) {
        const std::string &text = stored.string();
        // A string that would read as something else keeps its quote.
        if (numfmt::match_number(text) || val::parse_boolean(text) ||
            (!text.empty() && (text[0] == '=' || text[0] == '\'')))
            return "'" + text;
        return text;
    }
    if (stored.is_number()) {
        const auto format = numfmt::number_format::get(style_at(pos)->format);
        if (format->is_temporal() || format->family() ==
                                         numfmt::format_family::percent)
            return format->render(stored.number()).text;
        return stored.to_text();
    }
    return stored.to_text();
}

void sheet::apply_style(const cell_range &range, const style_patch &patch) {
    style_table &table = book_->styles();
    table.begin_patch();
    styles_.transform(range, [&](const style_ptr &existing) {
        return table.patched(existing, patch);
    });
}

void sheet::merge(const cell_range &range) {
    unmerge(range);
    merged_.push_back(range);
}

void sheet::unmerge(const cell_range &range) {
    std::erase_if(merged_, [&](const cell_range &item) {
        return item.overlaps(range);
    });
}

const cell_range *sheet::merged_at(cell_pos pos) const {
    for (const cell_range &item : merged_)
        if (item.contains(pos))
            return &item;
    return nullptr;
}

std::optional<cell_range> sheet::used_range() const {
    std::optional<cell_range> bounds;
    for (const auto &[col, cells] : columns_) {
        if (cells.empty())
            continue;
        const int first = cells.begin()->first;
        const int last = cells.rbegin()->first;
        if (!bounds) {
            bounds = cell_range{{col, first}, {col, last}};
            continue;
        }
        bounds->start.col = std::min(bounds->start.col, col);
        bounds->end.col = std::max(bounds->end.col, col);
        bounds->start.row = std::min(bounds->start.row, first);
        bounds->end.row = std::max(bounds->end.row, last);
    }
    return bounds;
}

void sheet::for_each_formula(const std::function<void(cell &)> &visit) {
    for (auto &[col, cells] : columns_)
        for (auto &[row, item] : cells)
            if (item.formula)
                visit(item);
}

void sheet::for_each_stored(const std::function<void(cell &)> &visit) {
    for (auto &[col, cells] : columns_)
        for (auto &[row, item] : cells)
            visit(item);
}

void sheet::set_dirty_mark(cell_pos pos, bool dirty) {
    if (dirty) {
        if (dirty_rows_[pos.col].insert(pos.row).second)
            ++dirty_count_;
        return;
    }
    const auto found = dirty_rows_.find(pos.col);
    if (found == dirty_rows_.end())
        return;
    if (found->second.erase(pos.row))
        --dirty_count_;
    if (found->second.empty())
        dirty_rows_.erase(found);
}

cell *sheet::first_dirty(const cell_range &range) {
    for (auto column_entry = dirty_rows_.lower_bound(range.start.col);
         column_entry != dirty_rows_.end() &&
         column_entry->first <= range.end.col;
         ++column_entry) {
        auto &rows = column_entry->second;
        for (auto row = rows.lower_bound(range.start.row);
             row != rows.end() && *row <= range.end.row; ++row) {
            cell *item = find({column_entry->first, *row});
            if (item && !item->being_calculated)
                return item;
        }
    }
    return nullptr;
}

void sheet::for_each_dirty(const cell_range &range,
                           const std::function<void(cell &)> &visit) {
    for (auto column_entry = dirty_rows_.lower_bound(range.start.col);
         column_entry != dirty_rows_.end() &&
         column_entry->first <= range.end.col;
         ++column_entry) {
        auto &rows = column_entry->second;
        for (auto row = rows.lower_bound(range.start.row);
             row != rows.end() && *row <= range.end.row; ++row)
            if (cell *item = find({column_entry->first, *row}))
                visit(*item);
    }
}

void sheet::remap_cells(
    const std::function<std::optional<cell_pos>(cell_pos)> &map) {
    // Moved cells keep their address; extraction preserves the node.
    struct moved_cell {
        std::map<int, cell>::node_type node;
        cell_pos target;
    };
    std::vector<moved_cell> moving;
    std::vector<cell_pos> doomed;
    for (auto &[col, cells] : columns_)
        for (auto &[row, item] : cells)
            if (!map(item.pos()))
                doomed.push_back(item.pos());
    for (const cell_pos pos : doomed)
        clear_cell(pos);
    std::vector<cell_pos> sources;
    for (auto &[col, cells] : columns_)
        for (auto &[row, item] : cells)
            if (map(item.pos()) != item.pos())
                sources.push_back(item.pos());
    for (const cell_pos pos : sources) {
        const auto target = map(pos);
        auto node = columns_[pos.col].extract(pos.row);
        node.mapped().set_pos(*target);
        if (node.mapped().in_array)
            node.mapped().array.corner =
                map(node.mapped().array.corner).value_or(*target);
        node.key() = target->row;
        moving.push_back({std::move(node), *target});
    }
    for (moved_cell &entry : moving)
        columns_[entry.target.col].insert(std::move(entry.node));
    std::erase_if(columns_,
                  [](const auto &entry) { return entry.second.empty(); });
}

void sheet::shift_rows_storage(int at, int count) {
    styles_.shift_rows(at, count);
    row_axis_.shift(at, count);
    std::vector<cell_range> kept;
    for (cell_range item : merged_) {
        if (count > 0) {
            if (item.start.row >= at)
                item.start.row += count;
            if (item.end.row >= at)
                item.end.row += count;
        } else {
            const int end = at - count;
            if (item.start.row >= at && item.end.row < end)
                continue;
            if (item.start.row >= end)
                item.start.row += count;
            else if (item.start.row > at)
                item.start.row = at;
            if (item.end.row >= end)
                item.end.row += count;
            else if (item.end.row >= at)
                item.end.row = at - 1;
        }
        if (!item.is_single() && item.end.row < rows_)
            kept.push_back(item);
    }
    merged_ = std::move(kept);
}

void sheet::shift_cols_storage(int at, int count) {
    styles_.shift_cols(at, count);
    col_axis_.shift(at, count);
    std::vector<cell_range> kept;
    for (cell_range item : merged_) {
        if (count > 0) {
            if (item.start.col >= at)
                item.start.col += count;
            if (item.end.col >= at)
                item.end.col += count;
        } else {
            const int end = at - count;
            if (item.start.col >= at && item.end.col < end)
                continue;
            if (item.start.col >= end)
                item.start.col += count;
            else if (item.start.col > at)
                item.start.col = at;
            if (item.end.col >= end)
                item.end.col += count;
            else if (item.end.col >= at)
                item.end.col = at - 1;
        }
        if (!item.is_single() && item.end.col < cols_)
            kept.push_back(item);
    }
    merged_ = std::move(kept);
}

} // namespace book
