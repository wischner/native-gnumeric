//
// Fonts and painting on Native. Gnumeric's default "Sans" and the common
// office fonts map to what Linux ships. Monochrome backends get no gray:
// grid lines dither in black, and the view knows from the palette.
//
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include "native_graphics.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace ui {
namespace {

constexpr int coordinate_limit = std::numeric_limits<native::coord>::max();

// Fonts tried, in order, when a family is missing.
constexpr std::array<std::string_view, 6> fallback_families{
    "DejaVu Sans",  "Liberation Sans", "Noto Sans",
    "DejaVu Serif", "Liberation Serif", "Noto Serif"};

// Replacements for fonts Linux rarely has, metric-compatible ones first.
constexpr std::array<std::pair<std::string_view, std::string_view>, 8>
    substitutes{{{"Sans", "DejaVu Sans"},
                 {"Serif", "DejaVu Serif"},
                 {"Monospace", "DejaVu Sans Mono"},
                 {"Times New Roman", "Liberation Serif"},
                 {"Arial", "Liberation Sans"},
                 {"Helvetica", "Liberation Sans"},
                 {"Courier New", "Liberation Mono"},
                 {"Courier", "Liberation Mono"}}};

// Report whether a style name marks a narrower or unusual cut.
bool is_special_cut(const std::string &style) {
    static constexpr std::array<std::string_view, 3> words{
        "Condensed", "Narrow", "Compressed"};
    return std::ranges::any_of(words, [&](std::string_view word) {
        return style.find(word) != std::string::npos;
    });
}

// Clamp a pixel coordinate into Native's coordinate range.
native::coord to_coord(int value) {
    return static_cast<native::coord>(
        std::clamp(value, -coordinate_limit, coordinate_limit));
}

// Convert a device rectangle to a Native one.
native::rect to_rect(const gfx::device_rect &bounds) {
    return native::rect(
        to_coord(bounds.x), to_coord(bounds.y),
        static_cast<native::dim>(std::clamp(bounds.width, 0, coordinate_limit)),
        static_cast<native::dim>(
            std::clamp(bounds.height, 0, coordinate_limit)));
}

// Convert a color to an opaque Native one.
native::rgba to_rgba(util::color color) {
    return native::rgba(color.r, color.g, color.b, 255);
}

// One face at one pixel size.
class native_font final : public gfx::font {
public:
    native_font(gfx::font_request request,
                const native::font_description *face)
        : request_(std::move(request)) {
        if (face)
            font_ = native::font_t::from_file(
                face->path,
                std::max(1, static_cast<int>(
                                std::lround(request_.size_pixels))),
                face->face_index, native::font_size_basis::em);
        const native::font_metrics metrics = active().get_metrics();
        ascent_ = metrics.ascent;
        descent_ = metrics.descent + metrics.leading;
    }

    const gfx::font_request &request() const override { return request_; }
    int ascent() const override { return ascent_; }
    int descent() const override { return descent_; }

    int measure(std::string_view text) const override {
        return active().measure_text(std::string(text)).advance;
    }

    // Return the font to draw with; stock when no face opened.
    const native::font_t &active() const {
        return font_.valid() ? font_
                             : native::font_t::stock(native::font_role::system);
    }

private:
    gfx::font_request request_;
    native::font_t font_;
    int ascent_ = 0;
    int descent_ = 0;
};

} // namespace

font_catalog::font_catalog() {
    for (native::font_description &description :
         native::font_t::enumerate_installed())
        if (!description.family.empty())
            by_family_[description.family].push_back(std::move(description));
    families_.reserve(by_family_.size());
    for (const auto &[family, faces] : by_family_)
        families_.push_back(family);
}

const native::font_description *
font_catalog::best_face(const gfx::font_request &request) const {
    std::vector<std::string> names{request.family};
    for (const auto &[wanted, replacement] : substitutes)
        if (wanted == request.family)
            names.emplace_back(replacement);
    names.insert(names.end(), fallback_families.begin(),
                 fallback_families.end());
    const int weight = request.bold ? 700 : 400;
    for (const std::string &name : names) {
        const auto family = by_family_.find(name);
        if (family == by_family_.end())
            continue;
        const native::font_description *best = nullptr;
        int best_score = std::numeric_limits<int>::max();
        for (const native::font_description &face : family->second) {
            const int score = std::abs(face.weight - weight) +
                              (face.italic != request.italic ? 1000 : 0) +
                              (is_special_cut(face.style) ? 500 : 0);
            if (score < best_score) {
                best = &face;
                best_score = score;
            }
        }
        if (best)
            return best;
    }
    return by_family_.empty() ? nullptr : &by_family_.begin()->second.front();
}

std::shared_ptr<const gfx::font>
font_catalog::find(const gfx::font_request &request) {
    const std::string key = request.family + '|' +
                            std::to_string(std::lround(request.size_pixels)) +
                            (request.bold ? "b" : "") +
                            (request.italic ? "i" : "");
    auto &entry = fonts_[key];
    if (!entry)
        entry = std::make_shared<native_font>(request, best_face(request));
    return entry;
}

native_painter::native_painter(native::gpx &target) : target_(target) {
    colors_.monochrome = native::resources::current_context().color ==
                         "monochrome";
    if (colors_.monochrome)
        colors_.grid = colors_.ink;
}

void native_painter::fill_rect(const gfx::device_rect &bounds,
                               util::color color) {
    if (selection_ink_)
        return;
    target_.set_ink(to_rgba(color)).draw_rect(to_rect(bounds), true);
}

void native_painter::frame_rect(const gfx::device_rect &bounds,
                                util::color color, int thickness) {
    target_.set_ink(selection_ink_.value_or(to_rgba(color)));
    target_.set_pen(1);
    for (int step = 0; step < thickness; ++step)
        target_.draw_rect(to_rect({bounds.x + step, bounds.y + step,
                                   bounds.width - 2 * step,
                                   bounds.height - 2 * step}),
                          false);
}

void native_painter::draw_line(int x1, int y1, int x2, int y2,
                               util::color color, int thickness) {
    target_.set_ink(selection_ink_.value_or(to_rgba(color)))
        .set_pen(static_cast<std::uint8_t>(std::clamp(thickness, 1, 16)));
    target_.draw_line(native::point(to_coord(x1), to_coord(y1)),
                      native::point(to_coord(x2), to_coord(y2)));
}

void native_painter::draw_text(const gfx::font &face, std::string_view text,
                               int x, int baseline, util::color color) {
    const native::font_t &font =
        static_cast<const native_font &>(face).active();
    target_.set_font(font).set_ink(selection_ink_.value_or(to_rgba(color)));
    target_.draw_text(std::string(text),
                      native::point(to_coord(x),
                                    to_coord(baseline - face.ascent())));
}

void native_painter::clipped(const gfx::device_rect &bounds,
                             const std::function<void()> &draw) {
    const native::rect clip = target_.get_clip().intersect(to_rect(bounds));
    if (!clip.w() || !clip.h())
        return;
    auto saved = target_.save_state();
    target_.set_clip(clip);
    draw();
}

void native_painter::draw_selection(
    const gfx::device_rect &bounds,
    const std::function<void(gfx::painter &)> &content) {
    const native::rect clip = target_.get_clip().intersect(to_rect(bounds));
    if (!clip.w() || !clip.h())
        return;
    auto saved = target_.save_state();
    target_.set_clip(clip);
    auto appearance = native::theme::create(target_);
    native::theme::state state;
    state.selected = true;
    appearance->draw_selection(to_rect(bounds), native::selection_shape::row,
                               state);
    native_painter selected(target_);
    selected.selection_ink_ = appearance->get_selection_foreground_color();
    content(selected);
}

} // namespace ui
