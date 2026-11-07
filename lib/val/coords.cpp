//
// A1 spelling of positions and ranges.
//
// Derived from Gnumeric src/parse-util.c,
// Copyright (C) 2000 Jody Goldberg and contributors.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include "val/coords.h"

namespace val {

std::string col_name(int col) {
    std::string name;
    for (int value = col + 1; value > 0; value = (value - 1) / 26)
        name.insert(name.begin(),
                    static_cast<char>('A' + (value - 1) % 26));
    return name;
}

std::string row_name(int row) {
    return std::to_string(row + 1);
}

std::string cell_name(cell_pos pos) {
    return col_name(pos.col) + row_name(pos.row);
}

std::string range_name(const cell_range &range) {
    if (range.is_single())
        return cell_name(range.start);
    return cell_name(range.start) + ":" + cell_name(range.end);
}

std::optional<int> parse_col(std::string_view letters) {
    if (letters.empty() || letters.size() > 6)
        return std::nullopt;
    int value = 0;
    for (const char c : letters) {
        const char upper = c >= 'a' && c <= 'z'
                               ? static_cast<char>(c - 32)
                               : c;
        if (upper < 'A' || upper > 'Z')
            return std::nullopt;
        value = value * 26 + (upper - 'A' + 1);
    }
    return value - 1;
}

std::optional<cell_pos> parse_cell_name(std::string_view text) {
    if (!text.empty() && text[0] == '$')
        text.remove_prefix(1);
    std::size_t split = 0;
    while (split < text.size() &&
           ((text[split] | 0x20) >= 'a' && (text[split] | 0x20) <= 'z'))
        ++split;
    const auto col = parse_col(text.substr(0, split));
    if (!col)
        return std::nullopt;
    text.remove_prefix(split);
    if (!text.empty() && text[0] == '$')
        text.remove_prefix(1);
    if (text.empty() || text.size() > 9)
        return std::nullopt;
    int row = 0;
    for (const char c : text) {
        if (c < '0' || c > '9')
            return std::nullopt;
        row = row * 10 + (c - '0');
    }
    if (row < 1)
        return std::nullopt;
    return cell_pos{*col, row - 1};
}

std::optional<cell_range> parse_range_name(std::string_view text) {
    const auto colon = text.find(':');
    const auto first = parse_cell_name(text.substr(0, colon));
    if (!first)
        return std::nullopt;
    if (colon == std::string_view::npos)
        return cell_range::single(*first);
    const auto second = parse_cell_name(text.substr(colon + 1));
    if (!second)
        return std::nullopt;
    return cell_range::between(*first, *second);
}

} // namespace val
