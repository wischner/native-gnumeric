//
// Autofill series, row sorting and text search.
//
// Derived from Gnumeric src/sheet-autofill.c, src/sort.c and src/search.c,
// Copyright (C) 1998-2014 Miguel de Icaza, Jody Goldberg, Morten Welinder,
// JP Rosevear.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include "book/fill.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>

#include "book/region.h"
#include "book/workbook.h"
#include "numfmt/date.h"
#include "numfmt/format.h"
#include "util/text.h"

namespace book {
namespace {

constexpr std::array<std::string_view, 12> month_names{
    "January", "February", "March",     "April",   "May",      "June",
    "July",    "August",   "September", "October", "November", "December"};
constexpr std::array<std::string_view, 7> day_names{
    "Monday", "Tuesday", "Wednesday", "Thursday",
    "Friday", "Saturday", "Sunday"};

// Find a text in a list of full or abbreviated names; -1 when absent.
template <std::size_t size>
int name_index(const std::array<std::string_view, size> &names,
               std::string_view text, bool &abbreviated) {
    for (std::size_t i = 0; i < size; ++i) {
        if (util::equal_nocase(names[i], text)) {
            abbreviated = false;
            return static_cast<int>(i);
        }
        if (util::equal_nocase(names[i].substr(0, 3), text)) {
            abbreviated = true;
            return static_cast<int>(i);
        }
    }
    return -1;
}

// One line of a fill, read along the fill direction.
struct fill_line {
    std::vector<cell_pos> sources;
    std::vector<cell_pos> targets;
};

// Copy a source cell's style and formula or constant to a target.
void copy_cell(sheet &where, cell_pos source, cell_pos target) {
    const region lifted = copy_region(where, cell_range::single(source));
    paste_region(where, target, lifted);
}

// Fill numbers with their constant step.
bool fill_numeric(sheet &where, const fill_line &line) {
    std::vector<double> numbers;
    for (const cell_pos pos : line.sources) {
        const cell *item = where.find(pos);
        if (!item || item->has_formula() || !item->result.is_number())
            return false;
        numbers.push_back(item->result.number());
    }
    if (numbers.empty())
        return false;
    // A single number repeats, as Gnumeric fills it.
    double step = 0;
    if (numbers.size() > 1) {
        // Least squares over the sources: exact for a true series.
        const double n = static_cast<double>(numbers.size());
        double sx = 0, sy = 0, sxy = 0, sxx = 0;
        for (std::size_t i = 0; i < numbers.size(); ++i) {
            sx += static_cast<double>(i);
            sy += numbers[i];
            sxy += static_cast<double>(i) * numbers[i];
            sxx += static_cast<double>(i * i);
        }
        step = (n * sxy - sx * sy) / (n * sxx - sx * sx);
    } else {
        const auto format =
            numfmt::number_format::get(where.style_at(line.sources[0])->format);
        if (format->is_temporal())
            step = 1;
    }
    const double last = numbers.back();
    int index = 1;
    for (const cell_pos target : line.targets) {
        where.styles().set(cell_range::single(target),
                           where.style_at(line.sources[0]));
        where.set_constant(target, last + step * index);
        ++index;
    }
    return true;
}

// Fill a cycle of day or month names.
bool fill_names(sheet &where, const fill_line &line) {
    const cell *first = where.find(line.sources[0]);
    if (!first || !first->result.is_string() || first->has_formula())
        return false;
    bool abbreviated = false;
    bool monthly = true;
    int index = name_index(month_names, first->result.string(), abbreviated);
    if (index < 0) {
        monthly = false;
        index = name_index(day_names, first->result.string(), abbreviated);
    }
    if (index < 0)
        return false;
    const int size = monthly ? 12 : 7;
    int step = 1;
    if (line.sources.size() >= 2) {
        const cell *second = where.find(line.sources[1]);
        if (!second || !second->result.is_string())
            return false;
        bool ignored = false;
        const int next =
            monthly ? name_index(month_names, second->result.string(), ignored)
                    : name_index(day_names, second->result.string(), ignored);
        if (next < 0)
            return false;
        step = ((next - index) % size + size) % size;
    }
    int current = index + step * static_cast<int>(line.sources.size() - 1);
    for (const cell_pos target : line.targets) {
        current += step;
        const std::string_view name =
            monthly ? month_names[static_cast<std::size_t>(current % 12)]
                    : day_names[static_cast<std::size_t>(current % 7)];
        where.styles().set(cell_range::single(target),
                           where.style_at(line.sources[0]));
        where.set_constant(target, value(abbreviated ? name.substr(0, 3)
                                                     : name));
    }
    return true;
}

// Fill text ending in a number: Item 1, Item 2.
bool fill_text_number(sheet &where, const fill_line &line) {
    const cell *last = where.find(line.sources.back());
    if (!last || !last->result.is_string() || last->has_formula())
        return false;
    const std::string &text = last->result.string();
    std::size_t split = text.size();
    while (split > 0 && text[split - 1] >= '0' && text[split - 1] <= '9')
        --split;
    if (split == text.size() || text.size() - split > 15)
        return false;
    const std::string prefix = text.substr(0, split);
    const std::size_t digits = text.size() - split;
    long long number = std::stoll(text.substr(split));
    for (const cell_pos target : line.targets) {
        ++number;
        std::string body = std::to_string(number);
        if (body.size() < digits)
            body.insert(0, digits - body.size(), '0');
        where.styles().set(cell_range::single(target),
                           where.style_at(line.sources[0]));
        where.set_constant(target, value(prefix + body));
    }
    return true;
}

// Repeat sources across the targets.
void fill_copy(sheet &where, const fill_line &line) {
    std::size_t index = 0;
    for (const cell_pos target : line.targets) {
        copy_cell(where, line.sources[index], target);
        index = (index + 1) % line.sources.size();
    }
}

// The cell text a search compares.
std::string searched_text(sheet &from, cell_pos pos,
                          const search_options &options) {
    const cell *item = from.find(pos);
    if (!item)
        return {};
    if (item->has_formula() && options.in_formulas)
        return from.entry_text(pos);
    if (options.in_values)
        return display_text(from, pos);
    return {};
}

// Report whether a text holds a needle under the options.
bool text_matches(std::string_view subject, std::string_view needle,
                  const search_options &options) {
    const std::string a =
        options.match_case ? std::string(subject) : util::to_lower(subject);
    const std::string b =
        options.match_case ? std::string(needle) : util::to_lower(needle);
    return options.whole_cell ? a == b : a.find(b) != std::string::npos;
}

// Rearrange whole lines of a rectangle by a permutation.
template <typename place_t>
void apply_order(sheet &where, const std::vector<int> &order, int count,
                 place_t line_range) {
    std::vector<region> lifted;
    lifted.reserve(order.size());
    for (int index = 0; index < count; ++index)
        lifted.push_back(copy_region(where, line_range(index)));
    for (int index = 0; index < count; ++index) {
        const cell_range target = line_range(index);
        paste_region(where, target.start,
                     lifted[static_cast<std::size_t>(
                         order[static_cast<std::size_t>(index)])]);
    }
}

// Compare two values in sort order: blanks last whichever the direction.
int compare_cells(const value &a, const value &b, const sort_key &key) {
    if (a.is_empty() || b.is_empty())
        return a.is_empty() == b.is_empty() ? 0 : (a.is_empty() ? 2 : -2);
    const val::comparison order = a.compare(b, key.case_sensitive);
    const int result = order == val::comparison::less      ? -1
                       : order == val::comparison::greater ? 1
                                                           : 0;
    return key.ascending ? result : -result;
}

} // namespace

void autofill(sheet &where, const cell_range &source,
              const cell_range &target) {
    const bool down = target.end.row > source.end.row;
    const bool up = target.start.row < source.start.row;
    const bool right = target.end.col > source.end.col;
    const bool left = target.start.col < source.start.col;
    if (!down && !up && !right && !left)
        return;
    const bool vertical = down || up;
    const int lines = vertical ? source.cols() : source.rows();
    for (int i = 0; i < lines; ++i) {
        fill_line line;
        if (vertical) {
            const int col = source.start.col + i;
            for (int row = source.start.row; row <= source.end.row; ++row)
                line.sources.push_back({col, row});
            if (down)
                for (int row = source.end.row + 1; row <= target.end.row;
                     ++row)
                    line.targets.push_back({col, row});
            else
                for (int row = source.start.row - 1; row >= target.start.row;
                     --row)
                    line.targets.push_back({col, row});
        } else {
            const int row = source.start.row + i;
            for (int col = source.start.col; col <= source.end.col; ++col)
                line.sources.push_back({col, row});
            if (right)
                for (int col = source.end.col + 1; col <= target.end.col;
                     ++col)
                    line.targets.push_back({col, row});
            else
                for (int col = source.start.col - 1; col >= target.start.col;
                     --col)
                    line.targets.push_back({col, row});
        }
        if (up || left)
            std::reverse(line.sources.begin(), line.sources.end());
        if (!fill_numeric(where, line) && !fill_names(where, line) &&
            !fill_text_number(where, line))
            fill_copy(where, line);
    }
}

void sort_rows(sheet &where, const cell_range &range,
               const std::vector<sort_key> &keys) {
    const int rows = range.rows();
    std::vector<int> order(static_cast<std::size_t>(rows));
    std::iota(order.begin(), order.end(), 0);
    std::vector<std::vector<value>> columns;
    for (const sort_key &key : keys) {
        std::vector<value> items;
        items.reserve(order.size());
        for (int row = 0; row < rows; ++row)
            items.push_back(where.value_at(
                {range.start.col + key.offset, range.start.row + row}));
        columns.push_back(std::move(items));
    }
    std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
        for (std::size_t i = 0; i < keys.size(); ++i) {
            const int result = compare_cells(columns[i][static_cast<std::size_t>(a)],
                                             columns[i][static_cast<std::size_t>(b)],
                                             keys[i]);
            if (result != 0)
                return result < 0;
        }
        return false;
    });
    apply_order(where, order, rows, [&](int index) {
        return cell_range{{range.start.col, range.start.row + index},
                          {range.end.col, range.start.row + index}};
    });
}

void sort_cols(sheet &where, const cell_range &range,
               const std::vector<sort_key> &keys) {
    const int cols = range.cols();
    std::vector<int> order(static_cast<std::size_t>(cols));
    std::iota(order.begin(), order.end(), 0);
    std::vector<std::vector<value>> lines;
    for (const sort_key &key : keys) {
        std::vector<value> items;
        for (int col = 0; col < cols; ++col)
            items.push_back(where.value_at(
                {range.start.col + col, range.start.row + key.offset}));
        lines.push_back(std::move(items));
    }
    std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
        for (std::size_t i = 0; i < keys.size(); ++i) {
            const int result = compare_cells(lines[i][static_cast<std::size_t>(a)],
                                             lines[i][static_cast<std::size_t>(b)],
                                             keys[i]);
            if (result != 0)
                return result < 0;
        }
        return false;
    });
    apply_order(where, order, cols, [&](int index) {
        return cell_range{{range.start.col + index, range.start.row},
                          {range.start.col + index, range.end.row}};
    });
}

std::optional<search_hit> find_text(sheet &from, cell_pos after,
                                    std::string_view text,
                                    const search_options &options,
                                    bool forward) {
    if (text.empty())
        return std::nullopt;
    std::vector<cell_pos> hits;
    from.for_each_stored([&](cell &item) {
        if (text_matches(searched_text(from, item.pos(), options), text,
                         options))
            hits.push_back(item.pos());
    });
    if (hits.empty())
        return std::nullopt;
    // Reading order is row by row.
    std::sort(hits.begin(), hits.end(), [](cell_pos a, cell_pos b) {
        return a.row != b.row ? a.row < b.row : a.col < b.col;
    });
    const auto later = [&](cell_pos a, cell_pos b) {
        return a.row != b.row ? a.row > b.row : a.col > b.col;
    };
    if (forward) {
        for (const cell_pos hit : hits)
            if (later(hit, after))
                return search_hit{&from, hit};
        return search_hit{&from, hits.front()};
    }
    for (auto hit = hits.rbegin(); hit != hits.rend(); ++hit)
        if (later(after, *hit))
            return search_hit{&from, *hit};
    return search_hit{&from, hits.back()};
}

bool replace_in_cell(sheet &where, cell_pos pos, std::string_view text,
                     std::string_view replacement,
                     const search_options &options) {
    const std::string entry = where.entry_text(pos);
    if (text.empty())
        return false;
    std::string result;
    if (options.whole_cell) {
        if (!text_matches(entry, text, options))
            return false;
        result = replacement;
    } else {
        const std::string subject =
            options.match_case ? entry : util::to_lower(entry);
        const std::string needle = options.match_case
                                       ? std::string(text)
                                       : util::to_lower(text);
        std::size_t at = 0;
        bool found = false;
        while (true) {
            const std::size_t hit = subject.find(needle, at);
            if (hit == std::string::npos)
                break;
            found = true;
            result.append(entry, at, hit - at);
            result += replacement;
            at = hit + needle.size();
        }
        if (!found)
            return false;
        result.append(entry, at, std::string::npos);
    }
    store_entry(where, pos, result);
    return true;
}

} // namespace book
