//
// Hash tables of reverse edges.
//
// Derived from Gnumeric src/dependent.c,
// Copyright (C) 2000-2006 Jody Goldberg, 1998-2002 Michael Meeks,
// 2000-2018 Morten Welinder.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include "book/dependents.h"

namespace book {

std::size_t dependents::place_hash::operator()(const place &item) const {
    const auto mixed = reinterpret_cast<std::uintptr_t>(item.sheet) ^
                       (static_cast<std::uintptr_t>(item.pos.col) << 20) ^
                       static_cast<std::uintptr_t>(item.pos.row);
    return std::hash<std::uintptr_t>{}(mixed);
}

std::size_t dependents::range_hash::operator()(const cell_range &item) const {
    std::size_t seed = static_cast<std::size_t>(item.start.col) * 31 +
                       static_cast<std::size_t>(item.start.row);
    seed = seed * 1000003 + static_cast<std::size_t>(item.end.col) * 31 +
           static_cast<std::size_t>(item.end.row);
    return seed;
}

void dependents::add(cell &formula) {
    for (const precedent &read : formula.reads) {
        if (read.range.is_single()) {
            singles_[{read.sheet, read.range.start}].insert(&formula);
            continue;
        }
        auto &buckets = ranges_[read.sheet];
        const int first = read.range.start.row >> bucket_shift;
        const int last = read.range.end.row >> bucket_shift;
        for (int index = first; index <= last; ++index)
            buckets[index][read.range].insert(&formula);
    }
}

void dependents::remove(cell &formula) {
    for (const precedent &read : formula.reads) {
        if (read.range.is_single()) {
            const auto found = singles_.find({read.sheet, read.range.start});
            if (found != singles_.end()) {
                found->second.erase(&formula);
                if (found->second.empty())
                    singles_.erase(found);
            }
            continue;
        }
        const auto sheet_ranges = ranges_.find(read.sheet);
        if (sheet_ranges == ranges_.end())
            continue;
        const int first = read.range.start.row >> bucket_shift;
        const int last = read.range.end.row >> bucket_shift;
        for (int index = first; index <= last; ++index) {
            const auto slot = sheet_ranges->second.find(index);
            if (slot == sheet_ranges->second.end())
                continue;
            const auto entry = slot->second.find(read.range);
            if (entry == slot->second.end())
                continue;
            entry->second.erase(&formula);
            if (entry->second.empty())
                slot->second.erase(entry);
        }
    }
}

void dependents::clear() {
    singles_.clear();
    ranges_.clear();
}

void dependents::for_each_reader(
    val::cell_source *sheet, cell_pos pos,
    const std::function<void(cell &)> &visit) const {
    const auto single = singles_.find({sheet, pos});
    if (single != singles_.end())
        for (cell *reader : single->second)
            visit(*reader);
    const auto sheet_ranges = ranges_.find(sheet);
    if (sheet_ranges == ranges_.end())
        return;
    const auto slot = sheet_ranges->second.find(pos.row >> bucket_shift);
    if (slot == sheet_ranges->second.end())
        return;
    for (const auto &[range, readers_of] : slot->second)
        if (range.contains(pos))
            for (cell *reader : readers_of)
                visit(*reader);
}

} // namespace book
