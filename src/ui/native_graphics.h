//
// Implements the gfx port on Native: fonts from installed faces and a
// painter over a gpx. This is the only bridge from the toolkit-free
// libraries to the UI toolkit.
//
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#ifndef UI_NATIVE_GRAPHICS_H
#define UI_NATIVE_GRAPHICS_H

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <native.h>

#include "gfx/graphics.h"

namespace ui {

// Finds faces among the installed fonts.
class font_catalog final : public gfx::font_provider {
public:
    // Enumerate installed fonts.
    font_catalog();

    // Return family names, sorted.
    const std::vector<std::string> &families() const { return families_; }

    std::shared_ptr<const gfx::font>
    find(const gfx::font_request &request) override;

private:
    const native::font_description *
    best_face(const gfx::font_request &request) const;

    std::map<std::string, std::vector<native::font_description>> by_family_;
    std::vector<std::string> families_;
    std::map<std::string, std::shared_ptr<const gfx::font>> fonts_;
};

// Paints into a Native graphics context.
class native_painter final : public gfx::painter {
public:
    // Create a painter for one paint.
    explicit native_painter(native::gpx &target);

    const gfx::palette &colors() const override { return colors_; }
    void fill_rect(const gfx::device_rect &bounds,
                   util::color color) override;
    void frame_rect(const gfx::device_rect &bounds, util::color color,
                    int thickness) override;
    void draw_line(int x1, int y1, int x2, int y2, util::color color,
                   int thickness) override;
    void draw_text(const gfx::font &face, std::string_view text, int x,
                   int baseline, util::color color) override;
    void clipped(const gfx::device_rect &bounds,
                 const std::function<void()> &draw) override;

    // Draw Native's active selection and clipped content in its foreground.
    void draw_selection(
        const gfx::device_rect &bounds,
        const std::function<void(gfx::painter &)> &content) override;

private:
    native::gpx &target_;
    gfx::palette colors_;
    std::optional<native::rgba> selection_ink_;
};

} // namespace ui

#endif
