//
// Offset arithmetic over default sizes and exceptions.
//
// Derived from Gnumeric src/colrow.c,
// Copyright (C) 1998-2003 Miguel de Icaza, Jody Goldberg, Morten Welinder.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include "book/axis.h"

#include <algorithm>
#include <cmath>

namespace book {

axis::axis(int count, double default_points)
    : count_(count), default_(default_points) {
    default_pixels_ = to_pixels(default_);
}

int axis::to_pixels(double points) const {
    return std::max(1, static_cast<int>(std::lround(points * scale_)));
}

void axis::set_default_points(double points) {
    default_ = points;
    default_pixels_ = to_pixels(points);
    stale_ = true;
}

double axis::points(int index) const {
    const auto found = entries_.find(index);
    return found == entries_.end() ? default_ : found->second.points;
}

bool axis::is_hard(int index) const {
    const auto found = entries_.find(index);
    return found != entries_.end() && found->second.hard;
}

bool axis::is_hidden(int index) const {
    const auto found = entries_.find(index);
    return found != entries_.end() && found->second.hidden;
}

void axis::set_points(int index, double points, bool hard) {
    axis_entry &entry = entries_[index];
    entry.points = points;
    entry.hard = hard;
    stale_ = true;
}

void axis::reset(int index) {
    entries_.erase(index);
    stale_ = true;
}

void axis::set_hidden(int index, bool hidden) {
    if (!entries_.count(index))
        entries_[index].points = default_;
    entries_[index].hidden = hidden;
    stale_ = true;
}

void axis::set_scale(double pixels_per_point) {
    scale_ = pixels_per_point;
    default_pixels_ = to_pixels(default_);
    stale_ = true;
}

int axis::pixels(int index) const {
    const auto found = entries_.find(index);
    if (found == entries_.end())
        return default_pixels_;
    return found->second.hidden ? 0 : to_pixels(found->second.points);
}

void axis::refresh() const {
    if (!stale_)
        return;
    marks_.clear();
    marks_.reserve(entries_.size());
    long long adjustment = 0;
    for (const auto &[index, entry] : entries_) {
        if (index >= count_)
            break;
        adjustment += pixels(index) - default_pixels_;
        marks_.push_back(
            {index, (index + 1LL) * default_pixels_ + adjustment});
    }
    stale_ = false;
}

long long axis::offset(int index) const {
    refresh();
    const auto after = std::lower_bound(
        marks_.begin(), marks_.end(), index,
        [](const mark &item, int wanted) { return item.index < wanted; });
    if (after == marks_.begin())
        return static_cast<long long>(index) * default_pixels_;
    const mark &previous = *std::prev(after);
    return previous.offset +
           (index - previous.index - 1LL) * default_pixels_;
}

int axis::index_at(long long position) const {
    if (position <= 0)
        return 0;
    refresh();
    const auto after = std::upper_bound(
        marks_.begin(), marks_.end(), position,
        [](long long wanted, const mark &item) {
            return wanted < item.offset;
        });
    if (after == marks_.begin()) {
        const long long index = position / default_pixels_;
        if (after != marks_.end() && index >= after->index)
            return after->index;
        return static_cast<int>(std::min<long long>(index, count_ - 1));
    }
    const mark &previous = *std::prev(after);
    const long long index = previous.index + 1 +
                            (position - previous.offset) / default_pixels_;
    if (after != marks_.end() && index >= after->index)
        return after->index;
    return static_cast<int>(std::min<long long>(index, count_ - 1));
}

void axis::shift(int at, int amount) {
    std::map<int, axis_entry> moved;
    for (const auto &[index, entry] : entries_) {
        if (index < at) {
            moved.emplace(index, entry);
        } else if (amount > 0) {
            if (index + amount < count_)
                moved.emplace(index + amount, entry);
        } else if (index >= at - amount) {
            moved.emplace(index + amount, entry);
        }
    }
    entries_ = std::move(moved);
    stale_ = true;
}

} // namespace book
