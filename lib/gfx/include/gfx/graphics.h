//
// The graphics port. The view measures through font_provider and paints
// through painter; neither interface knows any UI toolkit, so everything
// below src/ui builds and tests without a display. Writer's port with
// the same two halves works in layout units, because a document is laid
// out once for paper; a grid is laid out for the screen, so this one
// works in device pixels.
//
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#ifndef GFX_GRAPHICS_H
#define GFX_GRAPHICS_H

#include <functional>
#include <memory>
#include <string>
#include <string_view>

#include "util/color.h"

namespace gfx {

// The font a cell asks for.
struct font_request {
    std::string family;
    double size_pixels = 13;
    bool bold = false;
    bool italic = false;

    friend bool operator==(const font_request &,
                           const font_request &) = default;
};

// A resolved font, measured in device pixels.
class font {
public:
    virtual ~font() = default;

    // Return what was asked for.
    virtual const font_request &request() const = 0;

    // Return the distance from the baseline to the top of a line.
    virtual int ascent() const = 0;

    // Return the distance from the baseline to the bottom of a line.
    virtual int descent() const = 0;

    // Return the width of UTF-8 text on one line.
    virtual int measure(std::string_view text) const = 0;
};

// Finds fonts; results are shared and stay valid while held.
class font_provider {
public:
    virtual ~font_provider() = default;

    //
    // Return the closest available font.
    //
    // Notes:
    //      Never returns null: a workbook names fonts this machine may
    //      lack, and must still display.
    //
    virtual std::shared_ptr<const font>
    find(const font_request &request) = 0;
};

// A rectangle in device pixels.
struct device_rect {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;

    // Return the shared area of two rectangles; empty when none.
    device_rect intersect(const device_rect &other) const;

    // Report whether a point lies inside.
    bool contains(int px, int py) const {
        return px >= x && py >= y && px < x + width && py < y + height;
    }

    // Report whether the rectangle has no area.
    bool empty() const { return width <= 0 || height <= 0; }
};

// The colors the view takes from the platform.
struct palette {
    util::color paper{255, 255, 255};
    util::color ink{0, 0, 0};
    util::color grid{208, 208, 208};
    util::color frozen{136, 136, 136};
    bool monochrome = false;
};

// Draws in device pixels.
class painter {
public:
    virtual ~painter() = default;

    // Return the platform's colors for the grid.
    virtual const palette &colors() const = 0;

    // Fill a rectangle.
    virtual void fill_rect(const device_rect &bounds, util::color color) = 0;

    // Outline a rectangle, thickness growing inward.
    virtual void frame_rect(const device_rect &bounds, util::color color,
                            int thickness = 1) = 0;

    // Draw a line between two points.
    virtual void draw_line(int x1, int y1, int x2, int y2, util::color color,
                           int thickness = 1) = 0;

    //
    // Draw text with its origin on a baseline.
    //
    // Parameters:
    //      face        - A font from the same provider.
    //      text        - UTF-8, all on one line.
    //      x           - Left edge in device pixels.
    //      baseline    - Baseline in device pixels.
    //      color       - Ink.
    //
    virtual void draw_text(const font &face, std::string_view text, int x,
                           int baseline, util::color color) = 0;

    // Run a drawing callback clipped to a rectangle inside the current clip.
    virtual void clipped(const device_rect &bounds,
                         const std::function<void()> &draw) = 0;

    //
    // Paint the platform's selection, then redraw content in its ink.
    //
    // Parameters:
    //      bounds      - The selected area.
    //      content     - Redraws content through the painter it is given;
    //                    fills are suppressed there and text takes
    //                    selection ink.
    //
    virtual void draw_selection(
        const device_rect &bounds,
        const std::function<void(painter &)> &content) = 0;
};

// Measures with exact synthetic metrics, without a display.
class fixed_font_provider final : public font_provider {
public:
    std::shared_ptr<const font> find(const font_request &request) override;
};

} // namespace gfx

#endif
