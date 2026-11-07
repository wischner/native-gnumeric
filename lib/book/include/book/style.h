//
// Cell styles. A style is an immutable value, interned so equal styles
// share one object and comparing two is comparing pointers, the same
// idea as AbiWord's attribute table and Gnumeric's style hash. A style
// patch names only the fields it changes, which is what a toolbar
// button applies to a selection.
//
// Derived from Gnumeric src/mstyle.c and src/style-border.h,
// Copyright (C) 1998 Michael Meeks, Jody Goldberg, Morten Welinder.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#ifndef BOOK_STYLE_H
#define BOOK_STYLE_H

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "util/color.h"

namespace book {

// Horizontal alignment, with Gnumeric's stored values.
enum class h_align : int {
    general = 1,
    left = 2,
    right = 4,
    center = 8,
    fill = 16,
    justify = 32,
    center_across = 64,
    distributed = 128
};

// Vertical alignment, with Gnumeric's stored values.
enum class v_align : int {
    top = 1,
    bottom = 2,
    center = 4,
    justify = 8,
    distributed = 16
};

// A border line, with Gnumeric's stored values.
enum class border_line : int {
    none = 0,
    thin = 1,
    medium = 2,
    dashed = 3,
    dotted = 4,
    thick = 5,
    double_line = 6,
    hair = 7,
    medium_dash = 8,
    dash_dot = 9,
    medium_dash_dot = 10,
    dash_dot_dot = 11,
    medium_dash_dot_dot = 12,
    slanted_dash_dot = 13
};

// The sides of a cell, in Gnumeric's stored order.
enum class border_side : std::size_t {
    top,
    bottom,
    left,
    right,
    rev_diagonal,
    diagonal
};

inline constexpr std::size_t border_side_count = 6;

// One side's line.
struct border_edge {
    border_line line = border_line::none;
    util::color color;

    friend bool operator==(const border_edge &, const border_edge &) = default;
};

// Everything a cell's appearance depends on.
struct style {
    std::string font_name = "Sans";
    double font_size = 10;
    bool bold = false;
    bool italic = false;
    int underline = 0;
    bool strike = false;
    int script = 0;
    util::color fore{0, 0, 0};
    util::color back{255, 255, 255};
    util::color pattern_color{0, 0, 0};
    int pattern = 0;        // 0 none, 1 solid, more are shades.
    h_align horizontal = h_align::general;
    v_align vertical = v_align::bottom;
    bool wrap = false;
    bool shrink = false;
    int rotation = 0;
    int indent = 0;
    bool locked = true;
    bool hidden = false;
    std::string format = "General";
    std::array<border_edge, border_side_count> borders{};

    friend bool operator==(const style &, const style &) = default;
};

// A shared interned style.
using style_ptr = std::shared_ptr<const style>;

// Fields to change on existing styles.
struct style_patch {
    std::optional<std::string> font_name;
    std::optional<double> font_size;
    std::optional<bool> bold;
    std::optional<bool> italic;
    std::optional<int> underline;
    std::optional<bool> strike;
    std::optional<util::color> fore;
    std::optional<util::color> back;
    std::optional<int> pattern;
    std::optional<h_align> horizontal;
    std::optional<v_align> vertical;
    std::optional<bool> wrap;
    std::optional<int> indent;
    std::optional<std::string> format;
    std::array<std::optional<border_edge>, border_side_count> borders{};

    // Return a style with these changes applied.
    style apply(const style &base) const;
};

// Makes equal styles one object.
class style_table {
public:
    // Create a table holding the default style.
    style_table();

    // Return the shared object equal to a style.
    style_ptr intern(const style &item);

    // Return the default style.
    const style_ptr &default_style() const { return default_; }

    // Return a patched style through a cache keyed on the pair, because
    // a selection repeats the same few styles over thousands of cells.
    style_ptr patched(const style_ptr &base, const style_patch &patch);

    // Forget the patch cache; call when the patch changes.
    void begin_patch();

private:
    std::unordered_map<std::size_t, std::vector<style_ptr>> known_;
    std::unordered_map<const style *, style_ptr> patch_cache_;
    style_ptr default_;
};

} // namespace book

#endif
