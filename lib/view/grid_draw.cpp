//
// Paints the grid: backgrounds, grid lines, cell text with overflow into
// empty neighbors, borders, selection, the cursor and the open editor.
//
// Derived from Gnumeric src/item-grid.c, src/cell-draw.c and
// src/style-border.c,
// Copyright (C) 1998-2003 Miguel de Icaza, Jody Goldberg, Morten Welinder.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include <algorithm>
#include <cmath>

#include "numfmt/format.h"
#include "util/text.h"
#include "util/utf8.h"
#include "view/grid_view.h"

namespace view {

namespace {

constexpr int cell_margin = 2;

// Overflow looks this many columns left of the view for long text.
constexpr int overflow_reach = 64;

// Mix two colors, weight in tenths of the first.
util::color blend(util::color a, util::color b, int tenths) {
    const auto mix = [tenths](int x, int y) {
        return static_cast<std::uint8_t>((x * tenths + y * (10 - tenths)) /
                                         10);
    };
    return {mix(a.r, b.r), mix(a.g, b.g), mix(a.b, b.b)};
}

// Return a border's pixel thickness.
int border_width(book::border_line line) {
    switch (line) {
    case book::border_line::medium:
    case book::border_line::medium_dash:
    case book::border_line::medium_dash_dot:
    case book::border_line::medium_dash_dot_dot:
    case book::border_line::slanted_dash_dot:
        return 2;
    case book::border_line::thick:
        return 3;
    default:
        return 1;
    }
}

// Return a border's dash length; 0 draws it solid.
int border_dash(book::border_line line) {
    switch (line) {
    case book::border_line::dotted:
    case book::border_line::hair:
        return 1;
    case book::border_line::dashed:
    case book::border_line::medium_dash:
    case book::border_line::dash_dot:
    case book::border_line::medium_dash_dot:
    case book::border_line::dash_dot_dot:
    case book::border_line::medium_dash_dot_dot:
    case book::border_line::slanted_dash_dot:
        return 3;
    default:
        return 0;
    }
}

// Draw a horizontal or vertical line, dashed by a period.
void dashed_line(gfx::painter &target, int x1, int y1, int x2, int y2,
                 util::color color, int thickness, int dash) {
    if (dash == 0) {
        target.draw_line(x1, y1, x2, y2, color, thickness);
        return;
    }
    const bool horizontal = y1 == y2;
    const int length = horizontal ? x2 - x1 : y2 - y1;
    for (int at = 0; at < length; at += dash * 2) {
        const int end = std::min(at + dash - 1, length);
        if (horizontal)
            target.draw_line(x1 + at, y1, x1 + end, y1, color, thickness);
        else
            target.draw_line(x1, y1 + at, x1, y1 + end, color, thickness);
    }
}

} // namespace

// Paints one frame; lives for one call of grid_view::draw.
class grid_renderer {
public:
    grid_renderer(grid_view &source, gfx::painter &target)
        : view_(source), sheet_(source.active()), target_(target),
          colors_(target.colors()), scale_(source.zoom() * pixels_per_point) {
        visible_ = {0, 0, source.viewport_width(), source.viewport_height()};
        first_ = source.cell_at(source.scroll_x(), source.scroll_y());
        last_ = source.cell_at(source.scroll_x() + visible_.width,
                               source.scroll_y() + visible_.height);
    }

    void run() {
        target_.fill_rect(visible_, colors_.paper);
        backgrounds(target_);
        grid_lines();
        merged_cells();
        contents(target_);
        borders();
        selection();
        cursor();
        clipboard_frame();
        pointed_frame();
        fill_preview();
        editor();
    }

private:
    // Return a content rectangle in viewport pixels.
    gfx::device_rect on_screen(gfx::device_rect content) const {
        content.x -= view_.scroll_x();
        content.y -= view_.scroll_y();
        return content;
    }

    // Return a line's device offset in the viewport.
    int col_x(int col) const {
        return static_cast<int>(sheet_.cols().offset(col)) - view_.scroll_x();
    }

    int row_y(int row) const {
        return static_cast<int>(sheet_.rows().offset(row)) - view_.scroll_y();
    }

    void backgrounds(gfx::painter &into) {
        for (int row = first_.row; row <= last_.row; ++row)
            for (int col = first_.col; col <= last_.col; ++col) {
                const book::style &look = *sheet_.style_at({col, row});
                if (look.pattern == 0)
                    continue;
                const util::color fill =
                    look.pattern == 1
                        ? look.back
                        : blend(look.back, look.pattern_color, 7);
                into.fill_rect(on_screen(view_.range_rect(
                                   cell_range::single({col, row}))),
                               fill);
            }
    }

    void grid_lines() {
        if (!sheet_.show_grid)
            return;
        const int dash = colors_.monochrome ? 1 : 0;
        const int bottom = row_y(last_.row + 1);
        const int right = col_x(last_.col + 1);
        for (int col = first_.col; col <= last_.col + 1; ++col) {
            if (col < sheet_.col_count() && sheet_.cols().pixels(col) == 0)
                continue;
            const int x = col_x(col) - 1;
            dashed_line(target_, x, 0, x, std::min(bottom, visible_.height),
                        colors_.grid, 1, dash);
        }
        for (int row = first_.row; row <= last_.row + 1; ++row) {
            if (row < sheet_.row_count() && sheet_.rows().pixels(row) == 0)
                continue;
            const int y = row_y(row) - 1;
            dashed_line(target_, 0, y, std::min(right, visible_.width), y,
                        colors_.grid, 1, dash);
        }
    }

    // A merged area hides the grid lines inside it.
    void merged_cells() {
        for (const cell_range &merge : sheet_.merged()) {
            const gfx::device_rect area = on_screen(view_.range_rect(merge));
            if (area.intersect(visible_).empty())
                continue;
            const book::style &look = *sheet_.style_at(merge.start);
            target_.fill_rect({area.x, area.y, area.width - 1, area.height - 1},
                              look.pattern == 1 ? look.back : colors_.paper);
        }
    }

    // Return a cell's font at the zoom.
    std::shared_ptr<const gfx::font> face_of(const book::style &look) {
        return view_.fonts_->find({look.font_name, look.font_size * scale_,
                                   look.bold, look.italic});
    }

    // Report whether text may spill over this neighbor.
    bool blank(cell_pos pos) {
        const book::cell *found = sheet_.find(pos);
        return (!found || found->result.is_empty()) &&
               !sheet_.merged_at(pos);
    }

    // Return the text of a cell and the color it asks for.
    std::string text_of(cell_pos pos, const book::style &look,
                        const val::value &stored, int width,
                        util::color &ink, const gfx::font &face) {
        ink = look.fore;
        if (sheet_.show_formulas)
            return sheet_.entry_text(pos);
        if (stored.is_number()) {
            const int digit = std::max(1, face.measure("0"));
            const auto format = numfmt::number_format::get(look.format);
            const numfmt::rendered shown =
                format->render(stored.number(), width / digit);
            if (shown.color)
                ink = *shown.color;
            if (stored.number() == 0 && !sheet_.show_zeros)
                return {};
            return shown.text;
        }
        return book::display_text(sheet_, pos);
    }

    // Decide how a value lines up when the style says General.
    static book::h_align effective(const book::style &look,
                                   const val::value &stored) {
        if (look.horizontal != book::h_align::general)
            return look.horizontal;
        if (stored.is_number())
            return book::h_align::right;
        if (stored.is_bool() || stored.is_error())
            return book::h_align::center;
        return book::h_align::left;
    }

    // Split text into lines that fit a width.
    static std::vector<std::string> wrap_lines(const std::string &text,
                                               const gfx::font &face,
                                               int width) {
        std::vector<std::string> lines;
        std::string line;
        std::size_t at = 0;
        while (at <= text.size()) {
            const std::size_t space = text.find_first_of(" \n", at);
            const std::size_t end =
                space == std::string::npos ? text.size() : space;
            const std::string word = text.substr(at, end - at);
            const std::string joined = line.empty() ? word : line + " " + word;
            if (!line.empty() && face.measure(joined) > width) {
                lines.push_back(line);
                line = word;
            } else {
                line = joined;
            }
            if (space != std::string::npos && text[space] == '\n') {
                lines.push_back(line);
                line.clear();
            }
            at = end + 1;
        }
        lines.push_back(line);
        return lines;
    }

    // Lay one or more lines out inside an area and draw them.
    void draw_lines(gfx::painter &into, const std::vector<std::string> &lines,
                    const gfx::font &face, const gfx::device_rect &area,
                    book::h_align align, book::v_align vertical,
                    const book::style &look, util::color ink) {
        const int line_height = face.ascent() + face.descent();
        const int total = line_height * static_cast<int>(lines.size());
        int y = area.y + area.height - total - 1;
        if (vertical == book::v_align::top)
            y = area.y + 1;
        else if (vertical == book::v_align::center)
            y = area.y + (area.height - total) / 2;
        const int indent = look.indent * static_cast<int>(8 * scale_);
        for (const std::string &line : lines) {
            const int width = face.measure(line);
            int x = area.x + cell_margin + indent;
            if (align == book::h_align::right)
                x = area.x + area.width - width - cell_margin - 1 - indent;
            else if (align == book::h_align::center ||
                     align == book::h_align::center_across)
                x = area.x + (area.width - width) / 2;
            const int baseline = y + face.ascent();
            into.draw_text(face, line, x, baseline, ink);
            if (look.underline) {
                into.draw_line(x, baseline + 1, x + width, baseline + 1, ink);
            }
            if (look.strike) {
                const int middle = baseline - face.ascent() / 3;
                into.draw_line(x, middle, x + width, middle, ink);
            }
            y += line_height;
        }
    }

    // Draw one cell's content.
    void draw_cell(gfx::painter &into, cell_pos pos) {
        const book::cell *found = sheet_.find(pos);
        if (!found || sheet_.cols().pixels(pos.col) == 0 ||
            sheet_.rows().pixels(pos.row) == 0)
            return;
        const val::value &stored = sheet_.value_at(pos);
        if (stored.is_empty())
            return;
        const cell_range *merge = sheet_.merged_at(pos);
        if (merge && merge->start != pos)
            return;
        const book::style &look = *sheet_.style_at(pos);
        const gfx::device_rect cell = on_screen(view_.cell_rect(pos));
        const auto face = face_of(look);
        util::color ink = look.fore;
        const int room = cell.width - 2 * cell_margin - 1;
        const std::string text = text_of(pos, look, stored, room, ink, *face);
        if (text.empty())
            return;
        const book::h_align align = effective(look, stored);
        const bool numeric = stored.is_number() && !sheet_.show_formulas;
        if (look.wrap && !numeric) {
            into.clipped(cell, [&] {
                draw_lines(into, wrap_lines(text, *face, room), *face, cell,
                           align, look.vertical, look, ink);
            });
            return;
        }
        int width = face->measure(text);
        if (numeric && width > room) {
            // A number that does not fit must not mislead.
            const int count = std::max(1, room / std::max(1, face->measure("#")));
            into.clipped(cell, [&] {
                draw_lines(into, {std::string(static_cast<std::size_t>(count), '#')},
                           *face, cell, book::h_align::center, look.vertical,
                           look, ink);
            });
            return;
        }
        gfx::device_rect area = cell;
        if (!numeric && !merge)
            area = overflow_area(pos, cell, width, align);
        if (align == book::h_align::fill && width > 0) {
            // Repeat the text to fill the cell.
            std::string repeated;
            for (int i = 0; i < room / width; ++i)
                repeated += text;
            into.clipped(area, [&] {
                draw_lines(into, {repeated}, *face, area, book::h_align::left,
                           look.vertical, look, ink);
            });
            return;
        }
        into.clipped(area, [&] {
            draw_lines(into, {text}, *face, cell, align, look.vertical, look,
                       ink);
        });
    }

    // Grow a cell's area over the empty neighbors its text reaches.
    gfx::device_rect overflow_area(cell_pos pos, gfx::device_rect area,
                                   int width, book::h_align align) {
        const int left_col = pos.col;
        int need = width + 2 * cell_margin + 1 - area.width;
        if (need <= 0)
            return area;
        const bool both = align == book::h_align::center ||
                          align == book::h_align::center_across;
        if (align == book::h_align::left || both) {
            int reach = both ? need / 2 + 1 : need;
            for (int col = pos.col + 1;
                 reach > 0 && col < sheet_.col_count() &&
                 blank({col, pos.row});
                 ++col) {
                const int step = sheet_.cols().pixels(col);
                area.width += step;
                reach -= step;
            }
        }
        if (align == book::h_align::right || both) {
            int reach = both ? need / 2 + 1 : need;
            for (int col = left_col - 1;
                 reach > 0 && col >= 0 && blank({col, pos.row}); --col) {
                const int step = sheet_.cols().pixels(col);
                area.x -= step;
                area.width += step;
                reach -= step;
            }
        }
        return area;
    }

    // Draw the visible cells and what overflows into view.
    void contents(gfx::painter &into) {
        for (int row = first_.row; row <= last_.row; ++row) {
            // Long text left of the view can reach into it.
            int start = first_.col;
            for (int col = first_.col - 1;
                 col >= std::max(0, first_.col - overflow_reach); --col)
                if (sheet_.find({col, row})) {
                    start = col;
                    break;
                }
            for (int col = start; col <= last_.col; ++col)
                draw_cell(into, {col, row});
        }
    }

    // Draw one side of a cell's border.
    void border_side(const gfx::device_rect &area, book::border_side side,
                     const book::border_edge &edge) {
        if (edge.line == book::border_line::none)
            return;
        const int width = border_width(edge.line);
        const int dash = border_dash(edge.line);
        const int right = area.x + area.width - 1;
        const int bottom = area.y + area.height - 1;
        const int left = area.x - 1;
        const int top = area.y - 1;
        switch (side) {
        case book::border_side::top:
            dashed_line(target_, left, top, right, top, edge.color, width,
                        dash);
            break;
        case book::border_side::bottom:
            dashed_line(target_, left, bottom, right, bottom, edge.color,
                        width, dash);
            break;
        case book::border_side::left:
            dashed_line(target_, left, top, left, bottom, edge.color, width,
                        dash);
            break;
        case book::border_side::right:
            dashed_line(target_, right, top, right, bottom, edge.color, width,
                        dash);
            break;
        case book::border_side::diagonal:
            target_.draw_line(left, bottom, right, top, edge.color, width);
            break;
        case book::border_side::rev_diagonal:
            target_.draw_line(left, top, right, bottom, edge.color, width);
            break;
        }
    }

    void borders() {
        for (int row = first_.row; row <= last_.row; ++row)
            for (int col = first_.col; col <= last_.col; ++col) {
                const book::style &look = *sheet_.style_at({col, row});
                const gfx::device_rect area = on_screen(
                    view_.range_rect(cell_range::single({col, row})));
                for (std::size_t i = 0; i < look.borders.size(); ++i)
                    border_side(area, static_cast<book::border_side>(i),
                                look.borders[i]);
            }
    }

    void selection() {
        for (const cell_range &range : view_.ranges_) {
            if (range.is_single() && view_.ranges_.size() == 1)
                continue;
            if (const cell_range *merge = sheet_.merged_at(range.start);
                merge && *merge == range)
                continue;
            const gfx::device_rect area =
                on_screen(view_.range_rect(range)).intersect(visible_);
            if (area.empty())
                continue;
            target_.draw_selection(area, [&](gfx::painter &into) {
                contents(into);
            });
        }
    }

    void cursor() {
        const gfx::device_rect cell = on_screen(view_.cell_rect(view_.cursor_));
        // Inside a selection the active cell keeps its own colors.
        if (view_.has_range() && !cell.intersect(visible_).empty()) {
            const book::style &look = *sheet_.style_at(view_.cursor_);
            target_.clipped(cell, [&] {
                target_.fill_rect(cell, look.pattern == 1 ? look.back
                                                          : colors_.paper);
                draw_cell(target_, view_.cursor_);
            });
        }
        const gfx::device_rect frame =
            on_screen(view_.range_rect(view_.ranges_.back()));
        target_.frame_rect({frame.x - 1, frame.y - 1, frame.width + 1,
                            frame.height + 1},
                           colors_.ink, 2);
        target_.fill_rect({frame.x + frame.width - 3,
                           frame.y + frame.height - 3, 5, 5},
                          colors_.ink);
    }

    // Draw a dashed frame around a range.
    void dashed_frame(const gfx::device_rect &area) {
        const int right = area.x + area.width - 1;
        const int bottom = area.y + area.height - 1;
        dashed_line(target_, area.x - 1, area.y - 1, right, area.y - 1,
                    colors_.ink, 2, 4);
        dashed_line(target_, area.x - 1, bottom, right, bottom, colors_.ink, 2,
                    4);
        dashed_line(target_, area.x - 1, area.y - 1, area.x - 1, bottom,
                    colors_.ink, 2, 4);
        dashed_line(target_, right, area.y - 1, right, bottom, colors_.ink, 2,
                    4);
    }

    void clipboard_frame() {
        if (view_.clip_ && view_.clip_->sheet_name == sheet_.name())
            dashed_frame(on_screen(view_.range_rect(view_.clip_->range)));
    }

    void pointed_frame() {
        if (view_.pointed_)
            dashed_frame(on_screen(view_.range_rect(*view_.pointed_)));
    }

    void fill_preview() {
        if (view_.fill_preview_)
            dashed_frame(on_screen(view_.range_rect(*view_.fill_preview_)));
    }

    // Draw the open editor over its cell, growing to fit the text.
    void editor() {
        if (!view_.editing())
            return;
        const book::style &look = *sheet_.style_at(view_.cursor_);
        const auto face = face_of(look);
        const std::string text = view_.editor_.text();
        const gfx::device_rect cell = on_screen(view_.cell_rect(view_.cursor_));
        const int line_height = face->ascent() + face->descent();
        gfx::device_rect box{cell.x, cell.y,
                             std::max(cell.width,
                                      face->measure(text) +
                                          2 * cell_margin + 8),
                             std::max(cell.height, line_height + 2)};
        box.width = std::min(box.width, visible_.width - box.x);
        target_.fill_rect(box, colors_.paper);
        target_.frame_rect({box.x - 1, box.y - 1, box.width + 1, box.height + 1},
                           colors_.ink, 2);
        const int baseline = box.y + (box.height - line_height) / 2 +
                             face->ascent();
        target_.clipped(box, [&] {
            const int start = box.x + cell_margin;
            draw_edit_selection(*face, start, box);
            target_.draw_text(*face, text, start, baseline, look.fore);
            const std::string head = util::to_utf8(
                view_.editor_.characters().substr(0, view_.editor_.caret()));
            const int caret = start + face->measure(head);
            target_.draw_line(caret, box.y + 1, caret, box.y + box.height - 2,
                              colors_.ink);
        });
    }

    // Shade the selected part of the edit text.
    void draw_edit_selection(const gfx::font &face, int start,
                             const gfx::device_rect &box) {
        const cell_editor &edit = view_.editor_;
        if (!edit.has_selection())
            return;
        const std::size_t from = std::min(edit.caret(), edit.anchor());
        const std::size_t to = std::max(edit.caret(), edit.anchor());
        const std::u32string &all = edit.characters();
        const int x1 =
            start + face.measure(util::to_utf8(all.substr(0, from)));
        const int x2 = start + face.measure(util::to_utf8(all.substr(0, to)));
        target_.fill_rect({x1, box.y + 1, x2 - x1, box.height - 2},
                          colors_.grid);
    }

    grid_view &view_;
    sheet &sheet_;
    gfx::painter &target_;
    const gfx::palette &colors_;
    double scale_;
    gfx::device_rect visible_;
    cell_pos first_;
    cell_pos last_;
};

void grid_view::draw(gfx::painter &target) {
    grid_renderer(*this, target).run();
}

} // namespace view
