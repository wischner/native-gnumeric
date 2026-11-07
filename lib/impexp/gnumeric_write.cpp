//
// Writes the .gnumeric XML format, in the element order Gnumeric reads.
//
// Derived from Gnumeric src/xml-sax-write.c,
// Copyright (C) 2003-2007 Jody Goldberg, 2002-2021 Morten Welinder.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include <algorithm>
#include <cstdio>
#include <map>
#include <sstream>

#include "gnumeric_format.h"
#include "util/color.h"
#include "util/gzip.h"
#include "util/text.h"
#include "util/xml_reader.h"

namespace impexp {
namespace {

using book::cell;
using book::sheet;
using book::style;
using util::xml_escape;
using val::cell_range;

// Writes one file.
class writer {
public:
    explicit writer(book::workbook &source) : book_(source) {}

    std::string run() {
        out_ << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                "<gnm:Workbook xmlns:gnm=\"http://www.gnumeric.org/v10.dtd\" "
                "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
                "xsi:schemaLocation=\"http://www.gnumeric.org/v9.xsd\">\n"
                "  <gnm:Version Epoch=\"1\" Major=\"12\" Minor=\"60\" "
                "Full=\"1.12.60\"/>\n";
        write_attributes();
        out_ << book_.preserved_xml;
        write_calculation();
        write_sheet_index();
        write_names(nullptr);
        out_ << "  <gnm:Sheets>\n";
        for (int i = 0; i < book_.sheet_count(); ++i)
            write_sheet(book_.sheet_at(i));
        out_ << "  </gnm:Sheets>\n</gnm:Workbook>\n";
        return out_.str();
    }

private:
    void write_attributes() {
        if (book_.attributes.empty())
            return;
        out_ << "  <gnm:Attributes>\n";
        for (const auto &[name, item] : book_.attributes)
            out_ << "    <gnm:Attribute><gnm:name>" << xml_escape(name)
                 << "</gnm:name><gnm:value>" << xml_escape(item)
                 << "</gnm:value></gnm:Attribute>\n";
        out_ << "  </gnm:Attributes>\n";
    }

    void write_calculation() {
        out_ << "  <gnm:Calculation ManualRecalc=\""
             << (book_.automatic_recalculation ? 0 : 1)
             << "\" EnableIteration=\"" << (book_.iteration_enabled ? 1 : 0)
             << "\" MaxIterations=\"" << book_.iteration_limit
             << "\" IterationTolerance=\"" << book_.iteration_tolerance
             << "\" FloatRadix=\"2\" FloatDigits=\"53\"/>\n";
    }

    void write_sheet_index() {
        out_ << "  <gnm:SheetNameIndex>\n";
        for (int i = 0; i < book_.sheet_count(); ++i) {
            const sheet &item = book_.sheet_at(i);
            out_ << "    <gnm:SheetName gnm:Cols=\"" << item.col_count()
                 << "\" gnm:Rows=\"" << item.row_count() << "\">"
                 << xml_escape(item.name()) << "</gnm:SheetName>\n";
        }
        out_ << "  </gnm:SheetNameIndex>\n";
    }

    void write_names(sheet *scope) {
        bool any = false;
        for (const auto &name : book_.names()) {
            if (!name->definition || name->sheet != scope)
                continue;
            if (!any)
                out_ << "  <gnm:Names>\n";
            any = true;
            const expr::print_pos at{scope ? scope : nullptr, name->origin};
            out_ << "    <gnm:Name><gnm:name>" << xml_escape(name->name)
                 << "</gnm:name><gnm:value>"
                 << xml_escape(expr::to_text(*name->definition, at))
                 << "</gnm:value><gnm:position>"
                 << val::cell_name(name->origin)
                 << "</gnm:position></gnm:Name>\n";
        }
        if (any)
            out_ << "  </gnm:Names>\n";
    }

    void write_sheet(sheet &item) {
        const auto used = item.used_range();
        const auto bool_attr = [](bool flag) { return flag ? "1" : "0"; };
        out_ << "    <gnm:Sheet DisplayFormulas=\""
             << bool_attr(item.show_formulas) << "\" HideZero=\""
             << bool_attr(!item.show_zeros) << "\" HideGrid=\""
             << bool_attr(!item.show_grid) << "\" HideColHeader=\""
             << bool_attr(!item.show_col_header) << "\" HideRowHeader=\""
             << bool_attr(!item.show_row_header) << "\" Visibility=\""
             << (item.hidden ? "GNM_SHEET_VISIBILITY_HIDDEN"
                             : "GNM_SHEET_VISIBILITY_VISIBLE")
             << "\"";
        if (item.tab_color)
            out_ << " TabColor=\""
                 << util::format_gnumeric_color(*item.tab_color) << "\"";
        out_ << ">\n      <gnm:Name>" << xml_escape(item.name())
             << "</gnm:Name>\n      <gnm:MaxCol>"
             << (used ? used->end.col : -1) << "</gnm:MaxCol>\n      <gnm:MaxRow>"
             << (used ? used->end.row : -1) << "</gnm:MaxRow>\n      <gnm:Zoom>"
             << item.zoom << "</gnm:Zoom>\n";
        write_names(&item);
        write_styles(item);
        write_axis("Cols", "ColInfo", item.cols());
        write_axis("Rows", "RowInfo", item.rows());
        write_selection(item);
        write_cells(item);
        write_merges(item);
        out_ << item.preserved_xml;
        out_ << "    </gnm:Sheet>\n";
    }

    void write_styles(sheet &item) {
        out_ << "      <gnm:Styles>\n";
        item.styles().for_each_region(
            nullptr, [&](const cell_range &range, const book::style_ptr &run) {
                out_ << "        <gnm:StyleRegion startCol=\""
                     << range.start.col << "\" startRow=\"" << range.start.row
                     << "\" endCol=\"" << range.end.col << "\" endRow=\""
                     << range.end.row << "\">";
                write_style(*run);
                out_ << "</gnm:StyleRegion>\n";
            });
        out_ << "      </gnm:Styles>\n";
    }

    static std::string_view h_name(book::h_align item) {
        for (const auto &[spelled, entry] : gnumeric_h_aligns)
            if (entry == item)
                return spelled;
        return gnumeric_h_aligns[0].first;
    }

    static std::string_view v_name(book::v_align item) {
        for (const auto &[spelled, entry] : gnumeric_v_aligns)
            if (entry == item)
                return spelled;
        return gnumeric_v_aligns[0].first;
    }

    void write_style(const style &item) {
        out_ << "<gnm:Style HAlign=\"" << h_name(item.horizontal)
             << "\" VAlign=\"" << v_name(item.vertical) << "\" WrapText=\""
             << item.wrap << "\" ShrinkToFit=\"" << item.shrink
             << "\" Rotation=\"" << item.rotation << "\" Shade=\""
             << item.pattern << "\" Indent=\"" << item.indent
             << "\" Locked=\"" << item.locked << "\" Hidden=\"" << item.hidden
             << "\" Fore=\"" << util::format_gnumeric_color(item.fore)
             << "\" Back=\"" << util::format_gnumeric_color(item.back)
             << "\" PatternColor=\""
             << util::format_gnumeric_color(item.pattern_color)
             << "\" Format=\"" << xml_escape(item.format) << "\">"
             << "<gnm:Font Unit=\"" << item.font_size << "\" Bold=\""
             << item.bold << "\" Italic=\"" << item.italic
             << "\" Underline=\"" << item.underline << "\" StrikeThrough=\""
             << item.strike << "\" Script=\"" << item.script << "\">"
             << xml_escape(item.font_name) << "</gnm:Font>";
        bool any = false;
        for (std::size_t i = 0; i < item.borders.size(); ++i) {
            const book::border_edge &edge = item.borders[i];
            if (edge.line == book::border_line::none)
                continue;
            if (!any)
                out_ << "<gnm:StyleBorder>";
            any = true;
            out_ << "<gnm:" << gnumeric_border_names[i] << " Style=\""
                 << static_cast<int>(edge.line) << "\" Color=\""
                 << util::format_gnumeric_color(edge.color) << "\"/>";
        }
        if (any)
            out_ << "</gnm:StyleBorder>";
        out_ << "</gnm:Style>";
    }

    void write_axis(const char *group, const char *element,
                    const book::axis &item) {
        out_ << "      <gnm:" << group << " DefaultSizePts=\""
             << item.default_points() << "\">\n";
        for (const auto &[index, entry] : item.entries())
            out_ << "        <gnm:" << element << " No=\"" << index
                 << "\" Unit=\"" << entry.points << "\" HardSize=\""
                 << entry.hard << "\" Hidden=\"" << entry.hidden << "\"/>\n";
        out_ << "      </gnm:" << group << ">\n";
    }

    void write_selection(sheet &item) {
        const book::saved_selection &saved = item.selection;
        out_ << "      <gnm:Selections CursorCol=\"" << saved.cursor.col
             << "\" CursorRow=\"" << saved.cursor.row << "\">\n";
        for (const cell_range &range : saved.ranges)
            out_ << "        <gnm:Selection startCol=\"" << range.start.col
                 << "\" startRow=\"" << range.start.row << "\" endCol=\""
                 << range.end.col << "\" endRow=\"" << range.end.row
                 << "\"/>\n";
        out_ << "      </gnm:Selections>\n";
    }

    void write_cells(sheet &item) {
        out_ << "      <gnm:Cells>\n";
        std::map<std::pair<int, int>, cell *> ordered;
        item.for_each_stored([&](cell &stored) {
            // Array elements are recreated from their corner.
            if (stored.in_array && !stored.formula)
                return;
            ordered[{stored.pos().row, stored.pos().col}] = &stored;
        });
        for (const auto &[place, stored] : ordered)
            write_cell(item, *stored);
        out_ << "      </gnm:Cells>\n";
    }

    void write_cell(sheet &item, cell &stored) {
        out_ << "        <gnm:Cell Row=\"" << stored.pos().row << "\" Col=\""
             << stored.pos().col << "\"";
        if (stored.formula) {
            if (stored.in_array)
                out_ << " Rows=\"" << stored.array.rows << "\" Cols=\""
                     << stored.array.cols << "\"";
            out_ << ">"
                 << xml_escape("=" + expr::to_text(*stored.formula,
                                                   {&item, stored.pos()}))
                 << "</gnm:Cell>\n";
            return;
        }
        const val::value &constant = stored.result;
        int type = 10;
        std::string text;
        if (constant.is_bool()) {
            type = 20;
            text = constant.flag() ? "TRUE" : "FALSE";
        } else if (constant.is_number()) {
            type = 40;
            text = util::shortest_double(constant.number());
        } else if (constant.is_error()) {
            type = 50;
            text = constant.to_text();
        } else if (constant.is_string()) {
            type = 60;
            text = constant.string();
        }
        out_ << " ValueType=\"" << type << "\"";
        if (!stored.value_format.empty())
            out_ << " ValueFormat=\"" << xml_escape(stored.value_format)
                 << "\"";
        out_ << ">" << xml_escape(text) << "</gnm:Cell>\n";
    }

    void write_merges(sheet &item) {
        if (item.merged().empty())
            return;
        out_ << "      <gnm:MergedRegions>\n";
        for (const cell_range &range : item.merged())
            out_ << "        <gnm:Merge>" << val::range_name(range)
                 << "</gnm:Merge>\n";
        out_ << "      </gnm:MergedRegions>\n";
    }

    book::workbook &book_;
    std::ostringstream out_;
};

} // namespace

std::string write_gnumeric(book::workbook &source) {
    return util::gzip(writer(source).run());
}

} // namespace impexp
