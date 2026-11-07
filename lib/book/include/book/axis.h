//
// The sizes of a sheet's columns or rows. Sizes are stored in points as
// in the file, with exceptions from a default, and converted to device
// pixels at one scale; a table of cumulative offsets over the exceptions
// makes position lookups logarithmic even on a million rows. Gnumeric
// keeps segments of ColRowInfo for the same purpose.
//
// Derived from Gnumeric src/colrow.c,
// Copyright (C) 1998-2003 Miguel de Icaza, Jody Goldberg, Morten Welinder.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#ifndef BOOK_AXIS_H
#define BOOK_AXIS_H

#include <map>
#include <vector>

namespace book {

// The size of one column or row that differs from the default.
struct axis_entry {
    double points = 0;
    bool hard = false;      // Set by the user, not computed from content.
    bool hidden = false;
};

// Sizes along one axis.
class axis {
public:
    // Create an axis of equal items.
    axis(int count, double default_points);

    // Return the number of items.
    int count() const { return count_; }

    // Return the default size.
    double default_points() const { return default_; }

    // Change the default size.
    void set_default_points(double points);

    // Return an item's size in points; hidden items report their size.
    double points(int index) const;

    // Report whether an item was sized by hand.
    bool is_hard(int index) const;

    // Report whether an item is hidden.
    bool is_hidden(int index) const;

    // Set an item's size.
    void set_points(int index, double points, bool hard);

    // Forget an item's own size, returning it to the default.
    void reset(int index);

    // Hide or show an item.
    void set_hidden(int index, bool hidden);

    // Return the exceptions, for saving.
    const std::map<int, axis_entry> &entries() const { return entries_; }

    // Set the device pixels per point.
    void set_scale(double pixels_per_point);

    // Return an item's size in device pixels; hidden items are 0.
    int pixels(int index) const;

    // Return the device offset of an item's leading edge.
    long long offset(int index) const;

    // Return the item containing a device offset, clamped to the axis.
    int index_at(long long offset) const;

    // Return the device length of the whole axis.
    long long total() const { return offset(count_); }

    // Insert default-sized items; negative counts delete.
    void shift(int at, int amount);

private:
    // Return a size in pixels at the scale.
    int to_pixels(double points) const;

    // Rebuild the cumulative table when something changed.
    void refresh() const;

    // A cumulative record past one exception.
    struct mark {
        int index = 0;
        long long offset = 0;   // Offset of the item after this one.
    };

    int count_;
    double default_;
    double scale_ = 96.0 / 72.0;
    int default_pixels_ = 0;
    std::map<int, axis_entry> entries_;
    mutable std::vector<mark> marks_;
    mutable bool stale_ = true;
};

} // namespace book

#endif
