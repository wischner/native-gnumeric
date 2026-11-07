//
// The reverse edges of calculation: which formulas read a changed cell.
// Single cells are hashed. Ranges are hashed into buckets of rows, so a
// change looks only at the ranges overlapping its bucket, Gnumeric's
// scheme for the same reason: most ranges are small and local.
//
// Derived from Gnumeric src/dependent.c,
// Copyright (C) 2000-2006 Jody Goldberg, 1998-2002 Michael Meeks,
// 2000-2018 Morten Welinder.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#ifndef BOOK_DEPENDENTS_H
#define BOOK_DEPENDENTS_H

#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "book/cell.h"

namespace book {

// All reverse edges of a workbook.
class dependents {
public:
    // Record a formula's reads.
    void add(cell &formula);

    // Remove a formula's reads.
    void remove(cell &formula);

    // Forget every edge.
    void clear();

    // Visit the formulas that read a position.
    void for_each_reader(val::cell_source *sheet, cell_pos pos,
                         const std::function<void(cell &)> &visit) const;

private:
    // A cell on a sheet.
    struct place {
        val::cell_source *sheet;
        cell_pos pos;

        friend bool operator==(const place &, const place &) = default;
    };

    struct place_hash {
        std::size_t operator()(const place &item) const;
    };

    struct range_hash {
        std::size_t operator()(const cell_range &item) const;
    };

    using readers = std::unordered_set<cell *>;
    using bucket = std::unordered_map<cell_range, readers, range_hash>;

    // Rows of one range bucket: 2^10.
    static constexpr int bucket_shift = 10;

    std::unordered_map<place, readers, place_hash> singles_;
    std::unordered_map<val::cell_source *,
                       std::unordered_map<int, bucket>>
        ranges_;
};

} // namespace book

#endif
