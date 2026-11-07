//
// CSV in and out, and HTML out.
//
// Derived from Gnumeric src/stf-parse.c and plugins/html,
// Copyright (C) 1999-2004 Almer S. Tigelaar, Jody Goldberg, Rob Clark.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include <algorithm>
#include <sstream>

#include "book/region.h"
#include "gnumeric_format.h"
#include "util/color.h"
#include "util/xml_reader.h"

namespace impexp {
namespace {

using book::sheet;
using val::cell_pos;

// Split one CSV record starting at an offset; quotes may span lines.
std::vector<std::string> read_record(std::string_view data, std::size_t &at,
                                     char separator) {
    std::vector<std::string> fields(1);
    bool quoted = false;
    while (at < data.size()) {
        const char c = data[at++];
        if (quoted) {
            if (c == '"' && at < data.size() && data[at] == '"') {
                fields.back().push_back('"');
                ++at;
            } else if (c == '"') {
                quoted = false;
            } else {
                fields.back().push_back(c);
            }
        } else if (c == '"' && fields.back().empty()) {
            quoted = true;
        } else if (c == separator) {
            fields.emplace_back();
        } else if (c == '\n') {
            break;
        } else if (c != '\r') {
            fields.back().push_back(c);
        }
    }
    return fields;
}

// Choose the separator: the most common candidate in the first line.
char guess_separator(std::string_view data) {
    const std::string_view line = data.substr(0, data.find('\n'));
    char best = ',';
    long most = 0;
    for (const char candidate : {',', ';', '\t'}) {
        const long count = std::count(line.begin(), line.end(), candidate);
        if (count > most) {
            most = count;
            best = candidate;
        }
    }
    return best;
}

// Quote a field when it needs it.
std::string csv_field(const std::string &text) {
    if (text.find_first_of(",\"\n") == std::string::npos)
        return text;
    std::string out = "\"";
    for (const char c : text) {
        if (c == '"')
            out.push_back('"');
        out.push_back(c);
    }
    out.push_back('"');
    return out;
}

// Return HTML's name for an alignment, or nothing.
const char *html_align(book::h_align item) {
    switch (item) {
    case book::h_align::left:
        return "left";
    case book::h_align::right:
        return "right";
    case book::h_align::center:
    case book::h_align::center_across:
        return "center";
    case book::h_align::justify:
        return "justify";
    default:
        return nullptr;
    }
}

// Write the style attribute of a table cell.
std::string css_of(const book::style &item) {
    std::string css;
    if (item.bold)
        css += "font-weight:bold;";
    if (item.italic)
        css += "font-style:italic;";
    if (item.underline)
        css += "text-decoration:underline;";
    if (item.fore != util::color{})
        css += "color:" + util::format_css_color(item.fore) + ";";
    if (item.pattern == 1)
        css += "background:" + util::format_css_color(item.back) + ";";
    if (const char *align = html_align(item.horizontal))
        css += std::string("text-align:") + align + ";";
    return css;
}

} // namespace

import_result read_csv(std::string_view data, book::workbook &target) {
    import_result result;
    if (data.substr(0, 3) == "\xEF\xBB\xBF")
        data.remove_prefix(3);
    sheet &into = target.add_sheet("Sheet1");
    const char separator = guess_separator(data);
    std::size_t at = 0;
    int row = 0;
    while (at < data.size()) {
        if (row >= into.row_count()) {
            result.warnings.push_back("Rows past the end of the sheet were dropped");
            break;
        }
        const std::vector<std::string> fields =
            read_record(data, at, separator);
        for (int col = 0; col < static_cast<int>(fields.size()); ++col) {
            if (col >= into.col_count()) {
                result.warnings.push_back(
                    "Columns past the end of the sheet were dropped");
                break;
            }
            if (!fields[static_cast<std::size_t>(col)].empty())
                book::store_entry(into, {col, row},
                                  fields[static_cast<std::size_t>(col)]);
        }
        ++row;
    }
    target.recalculate();
    result.ok = true;
    return result;
}

std::string write_csv(book::workbook &source) {
    sheet &item = source.sheet_at(0);
    const auto used = item.used_range();
    std::string out;
    if (!used)
        return out;
    for (int row = 0; row <= used->end.row; ++row) {
        for (int col = 0; col <= used->end.col; ++col) {
            if (col)
                out.push_back(',');
            out += csv_field(book::display_text(item, {col, row}));
        }
        out.push_back('\n');
    }
    return out;
}

std::string write_html(book::workbook &source) {
    std::ostringstream out;
    out << "<!DOCTYPE html>\n<html>\n<head><meta charset=\"utf-8\">"
           "<title>Workbook</title></head>\n<body>\n";
    for (int i = 0; i < source.sheet_count(); ++i) {
        sheet &item = source.sheet_at(i);
        out << "<h2>" << util::xml_escape(item.name()) << "</h2>\n";
        const auto used = item.used_range();
        if (!used)
            continue;
        out << "<table border=\"1\" cellspacing=\"0\" cellpadding=\"3\">\n";
        for (int row = used->start.row; row <= used->end.row; ++row) {
            out << "<tr>";
            for (int col = used->start.col; col <= used->end.col; ++col) {
                const cell_pos pos{col, row};
                const val::cell_range *merge = item.merged_at(pos);
                if (merge && merge->start != pos)
                    continue;
                out << "<td";
                if (merge)
                    out << " colspan=\"" << merge->cols() << "\" rowspan=\""
                        << merge->rows() << "\"";
                const std::string css = css_of(*item.style_at(pos));
                if (!css.empty())
                    out << " style=\"" << css << "\"";
                out << ">" << util::xml_escape(book::display_text(item, pos))
                    << "</td>";
            }
            out << "</tr>\n";
        }
        out << "</table>\n";
    }
    out << "</body>\n</html>\n";
    return out.str();
}

} // namespace impexp
