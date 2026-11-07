//
// Copying and pasting rectangles.
//
// Derived from Gnumeric src/clipboard.c,
// Copyright (C) 1999-2008 Miguel de Icaza, Jody Goldberg, Morten Welinder.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include "book/region.h"

#include <algorithm>

#include "book/workbook.h"
#include "numfmt/format.h"

namespace book {
namespace {

// Return a shifted array formula's region cell at a target.
void place_cell(sheet &into, cell_pos target, const region_cell &item,
                const paste_options &options) {
    if (target.col >= into.col_count() || target.row >= into.row_count())
        return;
    if (options.values_only || (!item.formula && item.array_cols == 0)) {
        into.set_constant(target, item.constant, item.value_format);
        return;
    }
    if (item.array_cols > 0) {
        into.set_array({target,
                        {target.col + item.array_cols - 1,
                         target.row + item.array_rows - 1}},
                       item.formula);
        return;
    }
    into.set_formula(target, item.formula);
}

// Map a region-relative place through an optional transpose.
cell_pos placed(cell_pos corner, int col, int row, bool transpose) {
    return transpose ? cell_pos{corner.col + row, corner.row + col}
                     : cell_pos{corner.col + col, corner.row + row};
}

} // namespace

region copy_region(sheet &from, const cell_range &range) {
    region out;
    out.cols = range.cols();
    out.rows = range.rows();
    from.for_each_stored([&](cell &item) {
        if (!range.contains(item.pos()))
            return;
        // Array elements are recreated by their corner.
        if (item.in_array && !item.formula)
            return;
        region_cell lifted;
        lifted.col = item.pos().col - range.start.col;
        lifted.row = item.pos().row - range.start.row;
        lifted.constant = item.result;
        lifted.value_format = item.value_format;
        lifted.formula = item.formula;
        if (item.in_array) {
            lifted.array_cols = item.array.cols;
            lifted.array_rows = item.array.rows;
        }
        out.cells.push_back(std::move(lifted));
    });
    // Styles and merges, as runs of each column.
    for (int col = range.start.col; col <= range.end.col; ++col)
        from.styles().for_each_run(
            col, range.start.row, range.end.row,
            [&](int first, int last, const style_ptr &run) {
                const int x = col - range.start.col;
                out.styles.push_back({{{x, first - range.start.row},
                                       {x, last - range.start.row}},
                                      run});
            });
    for (const cell_range &merge : from.merged())
        if (range.contains(merge))
            out.merges.push_back(
                {{merge.start.col - range.start.col,
                  merge.start.row - range.start.row},
                 {merge.end.col - range.start.col,
                  merge.end.row - range.start.row}});
    return out;
}

void paste_region(sheet &into, cell_pos corner, const region &item,
                  const paste_options &options) {
    const int cols = options.transpose ? item.rows : item.cols;
    const int rows = options.transpose ? item.cols : item.rows;
    const cell_range target{
        corner,
        {std::min(corner.col + cols, into.col_count()) - 1,
         std::min(corner.row + rows, into.row_count()) - 1}};
    if (options.contents) {
        into.clear_range(target);
        into.unmerge(target);
        for (const region_cell &lifted : item.cells)
            place_cell(into,
                       placed(corner, lifted.col, lifted.row,
                              options.transpose),
                       lifted, options);
    }
    if (options.formats) {
        for (const region_style &run : item.styles) {
            const cell_pos a = placed(corner, run.range.start.col,
                                      run.range.start.row, options.transpose);
            const cell_pos b = placed(corner, run.range.end.col,
                                      run.range.end.row, options.transpose);
            if (a.col >= into.col_count() || a.row >= into.row_count())
                continue;
            into.styles().set(
                {a,
                 {std::min(b.col, into.col_count() - 1),
                  std::min(b.row, into.row_count() - 1)}},
                run.item);
        }
        for (const cell_range &merge : item.merges) {
            const cell_pos a = placed(corner, merge.start.col, merge.start.row,
                                      options.transpose);
            const cell_pos b = placed(corner, merge.end.col, merge.end.row,
                                      options.transpose);
            if (b.col < into.col_count() && b.row < into.row_count())
                into.merge({a, b});
        }
    }
}

void paste_repeated(sheet &into, const cell_range &target, const region &item,
                    const paste_options &options) {
    for (int row = target.start.row; row <= target.end.row; row += item.rows)
        for (int col = target.start.col; col <= target.end.col;
             col += item.cols)
            paste_region(into, {col, row}, item, options);
}

std::string display_text(sheet &from, cell_pos pos, int max_chars) {
    const value &stored = from.value_at(pos);
    if (stored.is_empty())
        return {};
    if (stored.is_error() || stored.is_bool())
        return stored.to_text();
    const auto format = numfmt::number_format::get(from.style_at(pos)->format);
    if (stored.is_string())
        return format->render_text(stored.string()).text;
    const cell *item = from.find(pos);
    if (item && !item->value_format.empty() && format->is_general())
        return numfmt::number_format::get(item->value_format)
            ->render(stored.number(), max_chars)
            .text;
    return format->render(stored.number(), max_chars).text;
}

std::string region_as_text(sheet &from, const cell_range &range) {
    std::string out;
    for (int row = range.start.row; row <= range.end.row; ++row) {
        for (int col = range.start.col; col <= range.end.col; ++col) {
            if (col > range.start.col)
                out.push_back('\t');
            out += display_text(from, {col, row});
        }
        out.push_back('\n');
    }
    return out;
}

void store_entry(sheet &into, cell_pos pos, std::string_view text) {
    if (text.empty()) {
        into.clear_cell(pos);
        return;
    }
    parsed_entry entry = into.parse_entry(pos, text);
    if (entry.formula)
        into.set_formula(pos, entry.formula);
    else
        into.set_constant(pos, entry.constant);
    if (!entry.format.empty() &&
        numfmt::number_format::get(into.style_at(pos)->format)->is_general()) {
        style_patch patch;
        patch.format = entry.format;
        into.apply_style(cell_range::single(pos), patch);
    }
}

void paste_text(sheet &into, cell_pos corner, std::string_view text) {
    int row = corner.row;
    while (!text.empty() && row < into.row_count()) {
        const auto line_end = text.find('\n');
        std::string_view line = text.substr(0, line_end);
        if (!line.empty() && line.back() == '\r')
            line.remove_suffix(1);
        int col = corner.col;
        while (col < into.col_count()) {
            const auto tab = line.find('\t');
            store_entry(into, {col, row}, line.substr(0, tab));
            ++col;
            if (tab == std::string_view::npos)
                break;
            line.remove_prefix(tab + 1);
        }
        ++row;
        if (line_end == std::string_view::npos)
            break;
        text.remove_prefix(line_end + 1);
    }
}

} // namespace book
