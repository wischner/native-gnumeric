//
// Exports the original Isaac toolbar drawings as color PNGs and binary
// ink masks. Run explicitly to regenerate the checked-in resource artwork.
//
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include <native.h>

#include <filesystem>
#include <iostream>

#include <array>
#include <utility>

namespace {

using native::gpx;
using native::point;
using native::rect;
using native::rgba;

constexpr rgba ink(40, 44, 52, 255);
constexpr rgba paper(255, 255, 255, 255);
constexpr rgba folder(226, 180, 72, 255);
constexpr rgba folder_edge(150, 110, 30, 255);
constexpr rgba disk(62, 104, 170, 255);
constexpr rgba accent(198, 60, 48, 255);
constexpr rgba arrow(46, 125, 80, 255);

// Draw a line between two points in one color.
void line(gpx &g, rgba color, int x1, int y1, int x2, int y2,
          int width = 1) {
    g.set_ink(color).set_pen(static_cast<std::uint8_t>(width));
    g.draw_line(point(static_cast<native::coord>(x1),
                      static_cast<native::coord>(y1)),
                point(static_cast<native::coord>(x2),
                      static_cast<native::coord>(y2)));
}

// Fill a rectangle.
void fill(gpx &g, rgba color, int x, int y, int width, int height) {
    g.set_ink(color).draw_rect(
        rect(static_cast<native::coord>(x), static_cast<native::coord>(y),
             static_cast<native::dim>(width),
             static_cast<native::dim>(height)),
        true);
}

// Outline a rectangle.
void frame(gpx &g, rgba color, int x, int y, int width, int height) {
    g.set_ink(color).set_pen(1);
    g.draw_rect(
        rect(static_cast<native::coord>(x), static_cast<native::coord>(y),
             static_cast<native::dim>(width),
             static_cast<native::dim>(height)),
        false);
}

// Draw a sheet of paper with a folded corner.
void sheet(gpx &g, int x, int y, int width, int height) {
    fill(g, paper, x, y, width, height);
    frame(g, ink, x, y, width, height);
    line(g, ink, x + width - 4, y, x + width - 1, y + 3);
}

void draw_new(gpx &g) {
    sheet(g, 3, 1, 10, 14);
}

void draw_open(gpx &g) {
    fill(g, folder, 1, 4, 13, 10);
    frame(g, folder_edge, 1, 4, 13, 10);
    fill(g, folder, 2, 2, 5, 2);
    line(g, folder_edge, 2, 2, 6, 2);
}

void draw_save(gpx &g) {
    fill(g, disk, 2, 2, 12, 12);
    fill(g, paper, 4, 3, 8, 5);
    fill(g, ink, 5, 10, 6, 4);
    fill(g, paper, 9, 11, 1, 2);
}

// Draw a curved arrow; direction is -1 for undo, 1 for redo.
void draw_turn(gpx &g, int direction, int width = 1) {
    const int middle = 8;
    const auto x = [&](int offset) { return middle + direction * offset; };
    line(g, arrow, x(-4), 11, x(2), 11, width);
    line(g, arrow, x(2), 11, x(4), 9, width);
    line(g, arrow, x(4), 9, x(4), 7, width);
    line(g, arrow, x(4), 7, x(2), 5, width);
    line(g, arrow, x(2), 5, x(-4), 5, width);
    line(g, arrow, x(-4), 5, x(-2), 3, width);
    line(g, arrow, x(-4), 5, x(-2), 7, width);
}

void draw_undo(gpx &g) {
    draw_turn(g, 1);
}

void draw_redo(gpx &g) {
    draw_turn(g, -1);
}

void draw_cut(gpx &g) {
    line(g, ink, 4, 2, 10, 11);
    line(g, ink, 11, 2, 5, 11);
    frame(g, accent, 2, 11, 4, 4);
    frame(g, accent, 9, 11, 4, 4);
}

void draw_copy(gpx &g) {
    sheet(g, 2, 1, 8, 10);
    sheet(g, 6, 5, 8, 10);
}

void draw_paste(gpx &g) {
    fill(g, folder, 2, 3, 11, 12);
    frame(g, folder_edge, 2, 3, 11, 12);
    fill(g, ink, 5, 1, 5, 3);
    sheet(g, 6, 7, 8, 8);
}

void draw_find(gpx &g) {
    g.set_ink(ink).set_pen(2);
    g.draw_ellipse(rect(2, 2, 9, 9), false);
    g.set_pen(1);
    line(g, ink, 10, 10, 14, 14);
    line(g, ink, 11, 10, 14, 13);
}

void draw_bold(gpx &g) {
    fill(g, ink, 4, 2, 3, 12);
    fill(g, ink, 4, 2, 7, 2);
    fill(g, ink, 4, 7, 7, 2);
    fill(g, ink, 4, 12, 8, 2);
    fill(g, ink, 10, 3, 2, 4);
    fill(g, ink, 11, 8, 2, 5);
}

void draw_italic(gpx &g) {
    line(g, ink, 7, 2, 12, 2);
    line(g, ink, 4, 13, 9, 13);
    line(g, ink, 10, 2, 6, 13);
    line(g, ink, 9, 2, 5, 13);
}

void draw_underline(gpx &g) {
    fill(g, ink, 4, 2, 2, 8);
    fill(g, ink, 10, 2, 2, 8);
    fill(g, ink, 5, 10, 6, 2);
    fill(g, accent, 3, 13, 10, 2);
}

void draw_strike(gpx &g) {
    line(g, ink, 11, 3, 6, 3);
    line(g, ink, 6, 3, 4, 5);
    line(g, ink, 4, 5, 6, 7);
    line(g, ink, 6, 7, 10, 8);
    line(g, ink, 10, 8, 12, 10);
    line(g, ink, 12, 10, 10, 12);
    line(g, ink, 10, 12, 5, 12);
    fill(g, accent, 2, 7, 12, 1);
}

// Draw four text lines given each one's start and length.
void text_lines(gpx &g, const std::array<std::pair<int, int>, 4> &lines) {
    int y = 3;
    for (const auto &[x, length] : lines) {
        line(g, ink, x, y, x + length, y);
        y += 3;
    }
}

void draw_left(gpx &g) {
    text_lines(g, {{{2, 11}, {2, 7}, {2, 11}, {2, 6}}});
}

void draw_center(gpx &g) {
    text_lines(g, {{{2, 11}, {4, 7}, {2, 11}, {5, 5}}});
}

void draw_right(gpx &g) {
    text_lines(g, {{{2, 11}, {6, 7}, {2, 11}, {7, 6}}});
}

void draw_justify(gpx &g) {
    text_lines(g, {{{2, 11}, {2, 11}, {2, 11}, {2, 11}}});
}

// The spreadsheet's own tools, in the same hand as Writer's.

void draw_sum(gpx &g) {
    line(g, ink, 3, 2, 12, 2, 2);
    line(g, ink, 3, 2, 8, 8, 1);
    line(g, ink, 4, 2, 9, 8, 1);
    line(g, ink, 8, 8, 3, 13, 1);
    line(g, ink, 9, 8, 4, 13, 1);
    line(g, ink, 3, 13, 12, 13, 2);
}

// Draw a sort arrow and rows of falling or rising length.
void draw_sort(gpx &g, bool ascending) {
    line(g, arrow, 3, 2, 3, 13);
    if (ascending) {
        line(g, arrow, 3, 13, 1, 11);
        line(g, arrow, 3, 13, 5, 11);
    } else {
        line(g, arrow, 3, 2, 1, 4);
        line(g, arrow, 3, 2, 5, 4);
    }
    int length = ascending ? 2 : 8;
    for (int y = 3; y <= 12; y += 3) {
        line(g, ink, 7, y, 7 + length, y);
        length += ascending ? 2 : -2;
    }
}

void draw_sort_up(gpx &g) {
    draw_sort(g, true);
}

void draw_sort_down(gpx &g) {
    draw_sort(g, false);
}

void draw_merge(gpx &g) {
    frame(g, ink, 1, 3, 14, 10);
    line(g, arrow, 3, 8, 12, 8);
    line(g, arrow, 3, 8, 5, 6);
    line(g, arrow, 3, 8, 5, 10);
    line(g, arrow, 12, 8, 10, 6);
    line(g, arrow, 12, 8, 10, 10);
}

void draw_percent(gpx &g) {
    frame(g, ink, 2, 2, 4, 4);
    frame(g, ink, 10, 10, 4, 4);
    line(g, accent, 12, 2, 3, 13);
}

void draw_currency(gpx &g) {
    line(g, arrow, 8, 1, 8, 14);
    line(g, ink, 11, 4, 6, 4);
    line(g, ink, 6, 4, 4, 6);
    line(g, ink, 4, 6, 6, 7);
    line(g, ink, 6, 7, 10, 8);
    line(g, ink, 10, 8, 12, 10);
    line(g, ink, 12, 10, 10, 11);
    line(g, ink, 10, 11, 5, 11);
}

// Draw a decimal point with digits, and an arrow for the direction.
void draw_decimals(gpx &g, bool more) {
    fill(g, ink, 2, 11, 2, 2);
    frame(g, ink, 6, 8, 3, 5);
    frame(g, ink, 11, 8, 3, 5);
    line(g, arrow, 4, 3, 13, 3);
    if (more) {
        line(g, arrow, 13, 3, 11, 1);
        line(g, arrow, 13, 3, 11, 5);
    } else {
        line(g, arrow, 4, 3, 6, 1);
        line(g, arrow, 4, 3, 6, 5);
    }
}

void draw_decimals_more(gpx &g) {
    draw_decimals(g, true);
}

void draw_decimals_less(gpx &g) {
    draw_decimals(g, false);
}

void draw_border(gpx &g) {
    frame(g, ink, 2, 2, 12, 12);
    for (int at = 4; at <= 12; at += 2) {
        line(g, accent, at, 8, at, 8);
        line(g, accent, 8, at, 8, at);
    }
}

using painter = void (*)(gpx &);

// Command names and their drawings.
constexpr std::array<std::pair<std::string_view, painter>, 26> icons{{
    {"fileNew", draw_new},
    {"fileOpen", draw_open},
    {"fileSave", draw_save},
    {"undo", draw_undo},
    {"redo", draw_redo},
    {"cut", draw_cut},
    {"copy", draw_copy},
    {"paste", draw_paste},
    {"find", draw_find},
    {"toggleBold", draw_bold},
    {"toggleItalic", draw_italic},
    {"toggleUline", draw_underline},
    {"toggleStrike", draw_strike},
    {"alignLeft", draw_left},
    {"alignCenter", draw_center},
    {"alignRight", draw_right},
    {"alignJustify", draw_justify},
    {"autoSum", draw_sum},
    {"sortAscending", draw_sort_up},
    {"sortDescending", draw_sort_down},
    {"mergeCells", draw_merge},
    {"formatPercent", draw_percent},
    {"formatCurrency", draw_currency},
    {"decimalsMore", draw_decimals_more},
    {"decimalsLess", draw_decimals_less},
    {"borderOutline", draw_border},
}};

} // namespace

// Export both variants without starting a window or using a toolkit API.
int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "Usage: export_icons OUTPUT_DIRECTORY\n";
        return 2;
    }
    try {
        const std::filesystem::path output(argv[1]);
        std::filesystem::create_directories(output / "color");
        std::filesystem::create_directories(output / "monochrome");
        for (const auto &[name, draw] : icons) {
            native::img image(16, 16);
            auto &graphics = image.get_gpx();
            graphics.set_clip(rect(0, 0, 16, 16));
            graphics.clear(rgba(0, 0, 0, 0));
            draw(graphics);
            const auto filename = std::string(name) + ".png";
            image.save(output / "color" / filename);
            // Two-pixel strokes retain the arrow shape when GEM applies
            // its disabled checkerboard. Keep the color artwork unchanged.
            if (name == "undo" || name == "redo") {
                graphics.clear(rgba(0, 0, 0, 0));
                draw_turn(graphics, name == "undo" ? 1 : -1, 2);
            }
            // Preserve outlines; remove paper and folder interiors so the
            // Native toolbar can tint all ink, including reverse video.
            for (int index = 0; index < 16 * 16; ++index) {
                auto &pixel = image.pixels()[index];
                const bool interior = pixel == paper || pixel == folder;
                pixel = rgba(0, 0, 0, pixel.a && !interior ? 255 : 0);
            }
            image.save(output / "monochrome" / filename);
        }
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
