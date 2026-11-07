//
// Style interning and patching.
//
// Derived from Gnumeric src/mstyle.c,
// Copyright (C) 1998 Michael Meeks, Jody Goldberg, Morten Welinder.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include "book/style.h"

#include <functional>

namespace book {
namespace {

// Mix one value into a hash.
void mix(std::size_t &seed, std::size_t value) {
    seed ^= value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
}

// Return a hash of the color.
std::size_t color_hash(util::color value) {
    return static_cast<std::size_t>(value.r) << 16 |
           static_cast<std::size_t>(value.g) << 8 | value.b;
}

// Return a hash of a style.
std::size_t style_hash(const style &item) {
    std::size_t seed = std::hash<std::string>{}(item.font_name);
    mix(seed, std::hash<std::string>{}(item.format));
    mix(seed, std::hash<double>{}(item.font_size));
    mix(seed, (item.bold ? 1u : 0u) | (item.italic ? 2u : 0u) |
                  (item.strike ? 4u : 0u) | (item.wrap ? 8u : 0u));
    mix(seed, color_hash(item.fore));
    mix(seed, color_hash(item.back));
    mix(seed, static_cast<std::size_t>(item.pattern));
    mix(seed, static_cast<std::size_t>(item.horizontal));
    mix(seed, static_cast<std::size_t>(item.vertical));
    for (const border_edge &edge : item.borders)
        mix(seed, static_cast<std::size_t>(edge.line));
    return seed;
}

} // namespace

style style_patch::apply(const style &base) const {
    style out = base;
    if (font_name)
        out.font_name = *font_name;
    if (font_size)
        out.font_size = *font_size;
    if (bold)
        out.bold = *bold;
    if (italic)
        out.italic = *italic;
    if (underline)
        out.underline = *underline;
    if (strike)
        out.strike = *strike;
    if (fore)
        out.fore = *fore;
    if (back)
        out.back = *back;
    if (pattern)
        out.pattern = *pattern;
    if (horizontal)
        out.horizontal = *horizontal;
    if (vertical)
        out.vertical = *vertical;
    if (wrap)
        out.wrap = *wrap;
    if (indent)
        out.indent = *indent;
    if (format)
        out.format = *format;
    for (std::size_t i = 0; i < border_side_count; ++i)
        if (borders[i])
            out.borders[i] = *borders[i];
    return out;
}

style_table::style_table() : default_(intern(style{})) {}

style_ptr style_table::intern(const style &item) {
    std::vector<style_ptr> &bucket = known_[style_hash(item)];
    for (const style_ptr &existing : bucket)
        if (*existing == item)
            return existing;
    bucket.push_back(std::make_shared<const style>(item));
    return bucket.back();
}

void style_table::begin_patch() {
    patch_cache_.clear();
}

style_ptr style_table::patched(const style_ptr &base,
                               const style_patch &patch) {
    const auto found = patch_cache_.find(base.get());
    if (found != patch_cache_.end())
        return found->second;
    style_ptr result = intern(patch.apply(*base));
    patch_cache_.emplace(base.get(), result);
    return result;
}

} // namespace book
