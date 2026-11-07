//
// Interval maps over rows, one per column.
//
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include "book/style_map.h"

#include <algorithm>

namespace book {

style_map::style_map(int cols, int rows, style_ptr base)
    : rows_(rows), columns_(static_cast<std::size_t>(cols)) {
    for (column &runs : columns_)
        runs.emplace(0, base);
}

const style_ptr &style_map::at(cell_pos pos) const {
    const column &runs = columns_[static_cast<std::size_t>(pos.col)];
    auto found = runs.upper_bound(pos.row);
    --found;
    return found->second;
}

void style_map::coalesce(column &runs, int row) {
    auto current = runs.find(row);
    if (current == runs.end() || current == runs.begin())
        return;
    if (std::prev(current)->second == current->second)
        runs.erase(current);
}

void style_map::set(const cell_range &range, const style_ptr &item) {
    transform(range, [&](const style_ptr &) { return item; });
}

void style_map::transform(
    const cell_range &range,
    const std::function<style_ptr(const style_ptr &)> &change) {
    for (int col = range.start.col; col <= range.end.col; ++col) {
        column &runs = columns_[static_cast<std::size_t>(col)];
        // Split the runs at both ends of the rectangle.
        for (const int row : {range.start.row, range.end.row + 1}) {
            if (row >= rows_ || runs.count(row))
                continue;
            auto found = runs.upper_bound(row);
            --found;
            runs.emplace(row, found->second);
        }
        for (auto run = runs.lower_bound(range.start.row);
             run != runs.end() && run->first <= range.end.row; ++run)
            run->second = change(run->second);
        for (auto run = runs.lower_bound(range.start.row);
             run != runs.end() && run->first <= range.end.row + 1;) {
            const auto next = std::next(run);
            coalesce(runs, run->first);
            run = next;
        }
    }
}

void style_map::shift_rows(int at, int count) {
    for (column &runs : columns_) {
        column moved;
        // New rows copy the row above; rows that come up from under a
        // deletion keep their style, and a sheet emptied keeps the last.
        const int probe = count > 0 ? at : std::min(at - count, rows_ - 1);
        auto covering = runs.upper_bound(probe);
        --covering;
        const style_ptr filler = covering->second;
        if (count > 0) {
            for (const auto &[row, item] : runs) {
                const int target = row >= at ? row + count : row;
                if (target < rows_)
                    moved.emplace(target, item);
            }
            if (!moved.count(at))
                moved.emplace(at, filler);
        } else {
            const int end = at - count;
            for (const auto &[row, item] : runs) {
                if (row < at)
                    moved.emplace(row, item);
                else if (row >= end)
                    moved.emplace(row + count, item);
            }
            if (at < rows_ && !moved.count(at))
                moved.emplace(at, filler);
        }
        if (moved.empty() || moved.begin()->first != 0)
            moved.emplace(0, moved.empty() ? filler : moved.begin()->second);
        runs = std::move(moved);
        for (auto run = runs.begin(); run != runs.end();) {
            const auto next = std::next(run);
            coalesce(runs, run->first);
            run = next;
        }
    }
}

void style_map::shift_cols(int at, int count) {
    const auto size = columns_.size();
    if (count > 0) {
        const column left = columns_[static_cast<std::size_t>(at)];
        columns_.insert(columns_.begin() + at, static_cast<std::size_t>(count),
                        left);
        columns_.resize(size);
    } else {
        const column last = columns_.back();
        columns_.erase(columns_.begin() + at, columns_.begin() + at - count);
        columns_.resize(size, last);
    }
}

void style_map::for_each_run(
    int col, int first, int last,
    const std::function<void(int, int, const style_ptr &)> &visit) const {
    const column &runs = columns_[static_cast<std::size_t>(col)];
    auto run = runs.upper_bound(first);
    --run;
    while (run != runs.end() && run->first <= last) {
        const auto next = std::next(run);
        const int end = next == runs.end() ? rows_ - 1 : next->first - 1;
        visit(std::max(run->first, first), std::min(end, last), run->second);
        run = next;
    }
}

void style_map::for_each_region(
    const style_ptr &skip,
    const std::function<void(const cell_range &, const style_ptr &)> &visit)
    const {
    const int cols = static_cast<int>(columns_.size());
    for (int col = 0; col < cols;) {
        // Columns with identical runs form one band.
        int last = col;
        while (last + 1 < cols &&
               columns_[static_cast<std::size_t>(last + 1)] ==
                   columns_[static_cast<std::size_t>(col)])
            ++last;
        const column &runs = columns_[static_cast<std::size_t>(col)];
        for (auto run = runs.begin(); run != runs.end(); ++run) {
            if (run->second == skip)
                continue;
            const auto next = std::next(run);
            const int end = next == runs.end() ? rows_ - 1 : next->first - 1;
            visit({{col, run->first}, {last, end}}, run->second);
        }
        col = last + 1;
    }
}

} // namespace book
