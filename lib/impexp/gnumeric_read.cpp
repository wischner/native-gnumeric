//
// Reads the .gnumeric XML format, gzipped or plain. A state stack keyed
// on element names replaces libxml's SAX tree of Gnumeric's reader. The
// parts of a sheet this build does not model are re-serialized from the
// events and saved back verbatim, so round trips through Isaac keep
// charts, print settings and the like.
//
// Derived from Gnumeric src/xml-sax-read.c,
// Copyright (C) 2000-2007 Jody Goldberg, 2002-2021 Morten Welinder.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include <algorithm>
#include <cstdlib>
#include <map>
#include <set>

#include "expr/parser.h"
#include "gnumeric_format.h"
#include "impexp/file_format.h"
#include "util/color.h"
#include "util/gzip.h"
#include "util/text.h"
#include "util/xml_reader.h"

namespace impexp {
namespace {

using book::sheet;
using book::style;
using util::xml_attributes;
using val::cell_pos;
using val::cell_range;
using val::error_code;
using val::value;

// Strip a namespace prefix from an element name.
std::string_view local_name(std::string_view name) {
    const auto colon = name.find(':');
    return colon == std::string_view::npos ? name : name.substr(colon + 1);
}

// Return an attribute as an integer.
int attr_int(const xml_attributes &attributes, std::string_view name,
             int fallback = 0) {
    const auto found = attributes.find(name);
    return found == attributes.end() ? fallback
                                     : std::atoi(found->second.c_str());
}

// Return an attribute as a number.
double attr_double(const xml_attributes &attributes, std::string_view name,
                   double fallback = 0) {
    const auto found = attributes.find(name);
    return found == attributes.end()
               ? fallback
               : std::strtod(found->second.c_str(), nullptr);
}

// Return an attribute as a boolean, 0/1 or TRUE/FALSE.
bool attr_bool(const xml_attributes &attributes, std::string_view name,
               bool fallback = false) {
    const auto found = attributes.find(name);
    if (found == attributes.end())
        return fallback;
    const std::string &text = found->second;
    return text == "1" || util::equal_nocase(text, "true");
}

// Return an attribute as text, or nothing.
std::optional<std::string> attr_text(const xml_attributes &attributes,
                                     std::string_view name) {
    const auto found = attributes.find(name);
    if (found == attributes.end())
        return std::nullopt;
    return found->second;
}

// Read an alignment name, or Gnumeric's older numeric form.
template <typename enum_t, std::size_t size>
std::optional<enum_t>
named_enum(const xml_attributes &attributes, std::string_view name,
           const std::array<std::pair<std::string_view, enum_t>, size> &table) {
    const auto text = attr_text(attributes, name);
    if (!text)
        return std::nullopt;
    for (const auto &[spelled, item] : table)
        if (*text == spelled)
            return item;
    const int numeric = std::atoi(text->c_str());
    for (const auto &[spelled, item] : table)
        if (static_cast<int>(item) == numeric)
            return item;
    return std::nullopt;
}

// Return a family from a font name; old files store an X11 name such as
// -adobe-helvetica-medium-r-normal--12-*.
std::string family_of(const std::string &name) {
    if (name.empty() || name[0] != '-')
        return name;
    const auto first = name.find('-', 1);
    if (first == std::string::npos)
        return name;
    const auto second = name.find('-', first + 1);
    std::string family = name.substr(first + 1, second - first - 1);
    if (!family.empty() && family != "*")
        family[0] = static_cast<char>(
            std::toupper(static_cast<unsigned char>(family[0])));
    return family == "*" ? "Sans" : family;
}

// Receives the events of one file.
class reader final : public util::xml_handler {
public:
    reader(book::workbook &book, import_result &result)
        : book_(book), result_(result) {}

    void start_element(std::string_view name,
                       const xml_attributes &attributes) override {
        text_.clear();
        const std::string_view local = local_name(name);
        if (preserve_depth_ > 0) {
            ++preserve_depth_;
            serialize_start(name, attributes);
            return;
        }
        if (in_sheet() && is_preserved_sheet_child(local) && depth_ == 3) {
            preserve_into_ = &sheet_->preserved_xml;
            preserve_depth_ = 1;
            serialize_start(name, attributes);
            return;
        }
        if (!sheet_ && depth_ == 1 && is_preserved_book_child(local)) {
            preserve_into_ = &book_.preserved_xml;
            preserve_depth_ = 1;
            serialize_start(name, attributes);
            return;
        }
        path_.emplace_back(local);
        ++depth_;
        dispatch_start(local, attributes);
    }

    void end_element(std::string_view name) override {
        if (preserve_depth_ > 0) {
            serialize_end(name);
            --preserve_depth_;
            return;
        }
        const std::string_view local = local_name(name);
        dispatch_end(local);
        path_.pop_back();
        --depth_;
        text_.clear();
    }

    void characters(std::string_view text) override {
        if (preserve_depth_ > 0)
            *preserve_into_ += util::xml_escape(text);
        else
            text_ += text;
    }

    // Parse the cells held until every sheet exists.
    void finish() {
        for (pending_cell &item : pending_)
            store(item);
        for (const pending_name &item : pending_names_)
            define(item);
    }

private:
    // One cell read, stored once every sheet name is known.
    struct pending_cell {
        sheet *where = nullptr;
        cell_pos pos;
        std::string text;
        int value_type = 0;
        int expr_id = 0;
        int array_cols = 0;
        int array_rows = 0;
        std::string value_format;
    };

    // One defined name.
    struct pending_name {
        sheet *scope = nullptr;
        std::string name;
        std::string value;
        cell_pos origin;
    };

    bool in_sheet() const { return sheet_ != nullptr; }

    bool parent_is(std::string_view name) const {
        return path_.size() >= 2 && path_[path_.size() - 2] == name;
    }

    // Report whether a sheet child is kept verbatim.
    static bool is_preserved_sheet_child(std::string_view name) {
        static const std::set<std::string_view> names{
            "PrintInformation", "Objects", "Filters", "Solver", "Scenarios",
            "SheetLayout", "Printer"};
        return names.count(name) > 0;
    }

    // Report whether a workbook child is kept verbatim.
    static bool is_preserved_book_child(std::string_view name) {
        return name == "document-meta" || name == "UIData" ||
               name == "Summary";
    }

    // Append a start tag to the preserved text.
    void serialize_start(std::string_view name,
                         const xml_attributes &attributes) {
        *preserve_into_ += '<';
        *preserve_into_ += name;
        for (const auto &[key, item] : attributes) {
            *preserve_into_ += ' ';
            *preserve_into_ += key;
            *preserve_into_ += "=\"";
            *preserve_into_ += util::xml_escape(item);
            *preserve_into_ += '"';
        }
        *preserve_into_ += '>';
    }

    // Append an end tag to the preserved text.
    void serialize_end(std::string_view name) {
        *preserve_into_ += "</";
        *preserve_into_ += name;
        *preserve_into_ += '>';
    }

    void dispatch_start(std::string_view local,
                        const xml_attributes &attributes) {
        if (local == "Sheet")
            begin_sheet(attributes);
        else if (local == "SheetName")
            begin_sheet_name(attributes);
        else if (local == "Calculation")
            read_calculation(attributes);
        else if (!sheet_)
            return;
        else if (local == "Cell")
            begin_cell(attributes);
        else if (local == "StyleRegion")
            begin_region(attributes);
        else if (local == "Style")
            read_style(attributes);
        else if (local == "Font")
            read_font(attributes);
        else if (local == "ColInfo")
            read_axis(sheet_->cols(), attributes);
        else if (local == "RowInfo")
            read_axis(sheet_->rows(), attributes);
        else if (local == "Cols")
            sheet_->cols().set_default_points(
                attr_double(attributes, "DefaultSizePts", 48));
        else if (local == "Rows")
            sheet_->rows().set_default_points(
                attr_double(attributes, "DefaultSizePts", 12.75));
        else if (local == "Selections")
            read_cursor(attributes);
        else if (local == "Selection")
            read_selection(attributes);
        else if (local == "Merge")
            ;
        else
            read_style_extra(local, attributes);
    }

    void dispatch_end(std::string_view local) {
        if (local == "Sheet") {
            sheet_ = nullptr;
            ++sheet_index_;
        } else if (local == "SheetName") {
            end_sheet_name();
        } else if (local == "Attribute" || local == "name" ||
                   local == "value" || local == "position") {
            end_name_part(local);
        } else if (local == "Name" && parent_is("Names")) {
            end_defined_name();
        } else if (local == "Name" && sheet_) {
            sheet_->set_name(text_);
        } else if (local == "Zoom" && sheet_) {
            sheet_->zoom = std::strtod(text_.c_str(), nullptr);
        } else if (local == "Content" && sheet_) {
            // Files from before version 1.0 nest the text one level down.
            content_ = text_;
        } else if (local == "Cell" && sheet_) {
            end_cell();
        } else if (local == "StyleRegion" && sheet_) {
            end_region();
        } else if (local == "Font" && sheet_) {
            if (!text_.empty())
                current_.font_name = family_of(text_);
        } else if (local == "Merge" && sheet_) {
            if (const auto range = val::parse_range_name(text_))
                sheet_->merge(*range);
        }
    }

    // SheetNameIndex comes first and declares sizes.
    void begin_sheet_name(const xml_attributes &attributes) {
        declared_cols_ = attr_int(attributes, "gnm:Cols", val::default_cols);
        declared_rows_ = attr_int(attributes, "gnm:Rows", val::default_rows);
    }

    void end_sheet_name() {
        book_.add_sheet(text_, declared_cols_, declared_rows_);
    }

    void read_calculation(const xml_attributes &attributes) {
        book_.iteration_enabled = attr_bool(attributes, "EnableIteration", true);
        book_.iteration_limit = attr_int(attributes, "MaxIterations", 100);
        book_.iteration_tolerance =
            attr_double(attributes, "IterationTolerance", 0.001);
        book_.automatic_recalculation =
            !attr_bool(attributes, "ManualRecalc", false);
    }

    void begin_sheet(const xml_attributes &attributes) {
        if (sheet_index_ < book_.sheet_count()) {
            sheet_ = &book_.sheet_at(sheet_index_);
        } else {
            // A file from before SheetNameIndex.
            sheet_ = &book_.add_sheet(book_.unique_sheet_name(),
                                      attr_int(attributes, "Cols",
                                               val::default_cols),
                                      attr_int(attributes, "Rows",
                                               val::default_rows));
        }
        sheet_->show_grid = !attr_bool(attributes, "HideGrid");
        sheet_->show_formulas = attr_bool(attributes, "DisplayFormulas");
        sheet_->show_zeros = !attr_bool(attributes, "HideZero");
        sheet_->show_col_header = !attr_bool(attributes, "HideColHeader");
        sheet_->show_row_header = !attr_bool(attributes, "HideRowHeader");
        sheet_->hidden = attr_text(attributes, "Visibility")
                             .value_or("GNM_SHEET_VISIBILITY_VISIBLE") !=
                         "GNM_SHEET_VISIBILITY_VISIBLE";
        if (const auto color = attr_text(attributes, "TabColor"))
            sheet_->tab_color = util::parse_gnumeric_color(*color);
    }

    void begin_cell(const xml_attributes &attributes) {
        cell_ = {};
        content_.reset();
        cell_.where = sheet_;
        cell_.pos = {attr_int(attributes, "Col"), attr_int(attributes, "Row")};
        cell_.value_type = attr_int(attributes, "ValueType");
        cell_.expr_id = attr_int(attributes, "ExprID");
        cell_.array_cols = attr_int(attributes, "Cols");
        cell_.array_rows = attr_int(attributes, "Rows");
        cell_.value_format =
            attr_text(attributes, "ValueFormat").value_or("");
    }

    void end_cell() {
        cell_.text = content_ ? *content_ : text_;
        content_.reset();
        if (cell_.pos.col >= sheet_->col_count() ||
            cell_.pos.row >= sheet_->row_count())
            return;
        pending_.push_back(std::move(cell_));
    }

    // Store one cell's constant, formula or shared formula.
    void store(pending_cell &item) {
        sheet &where = *item.where;
        if (item.expr_id > 0) {
            const auto found = shared_.find(item.expr_id);
            if (found == shared_.end()) {
                if (item.text.empty())
                    return;
            } else if (item.text.empty()) {
                where.set_formula(item.pos, found->second.formula);
                return;
            }
        }
        if (item.value_type == 0 && !item.text.empty() &&
            item.text[0] == '=') {
            store_formula(item);
            return;
        }
        value constant;
        switch (item.value_type) {
        case 20:
            constant = value(val::parse_boolean(item.text).value_or(false));
            break;
        case 30:
        case 40:
            constant = value(std::strtod(item.text.c_str(), nullptr));
            break;
        case 50:
            constant = value(val::parse_error(item.text)
                                 .value_or(error_code::unknown));
            break;
        case 60:
            constant = value(item.text);
            break;
        default:
            if (!item.text.empty())
                constant = value(item.text);
            break;
        }
        where.set_constant(item.pos, std::move(constant), item.value_format);
    }

    void store_formula(pending_cell &item) {
        sheet &where = *item.where;
        const expr::parse_pos at{&where, item.pos, &book_};
        expr::parse_result parsed = expr::parse_formula(item.text, at);
        if (!parsed.tree) {
            result_.warnings.push_back("Cell " + where.name() + "!" +
                                       val::cell_name(item.pos) +
                                       ": formula kept as text: " + item.text);
            where.set_constant(item.pos, value(item.text));
            return;
        }
        if (item.array_cols > 0 && item.array_rows > 0) {
            const cell_range range{
                item.pos,
                {item.pos.col + item.array_cols - 1,
                 item.pos.row + item.array_rows - 1}};
            where.set_array(range, parsed.tree);
            return;
        }
        if (item.expr_id > 0)
            shared_[item.expr_id] = {parsed.tree, item.pos};
        where.set_formula(item.pos, parsed.tree);
    }

    void begin_region(const xml_attributes &attributes) {
        region_ = {{attr_int(attributes, "startCol"),
                    attr_int(attributes, "startRow")},
                   {attr_int(attributes, "endCol"),
                    attr_int(attributes, "endRow")}};
        current_ = style{};
    }

    void read_style(const xml_attributes &attributes) {
        if (const auto item =
                named_enum(attributes, "HAlign", gnumeric_h_aligns))
            current_.horizontal = *item;
        if (const auto item =
                named_enum(attributes, "VAlign", gnumeric_v_aligns))
            current_.vertical = *item;
        current_.wrap = attr_bool(attributes, "WrapText");
        current_.shrink = attr_bool(attributes, "ShrinkToFit");
        current_.rotation = attr_int(attributes, "Rotation");
        current_.pattern = attr_int(attributes, "Shade");
        current_.indent = attr_int(attributes, "Indent");
        current_.locked = attr_bool(attributes, "Locked", true);
        current_.hidden = attr_bool(attributes, "Hidden");
        read_color(attributes, "Fore", current_.fore);
        read_color(attributes, "Back", current_.back);
        read_color(attributes, "PatternColor", current_.pattern_color);
        if (const auto format = attr_text(attributes, "Format"))
            current_.format = *format;
    }

    static void read_color(const xml_attributes &attributes,
                           std::string_view name, util::color &target) {
        if (const auto text = attr_text(attributes, name))
            if (const auto color = util::parse_gnumeric_color(*text))
                target = *color;
    }

    void read_font(const xml_attributes &attributes) {
        current_.font_size = attr_double(attributes, "Unit", 10);
        current_.bold = attr_int(attributes, "Bold") != 0;
        current_.italic = attr_int(attributes, "Italic") != 0;
        current_.underline = attr_int(attributes, "Underline");
        current_.strike = attr_int(attributes, "StrikeThrough") != 0;
        current_.script = attr_int(attributes, "Script");
    }

    // Border elements and style features only warned about.
    void read_style_extra(std::string_view local,
                          const xml_attributes &attributes) {
        for (std::size_t i = 0; i < gnumeric_border_names.size(); ++i)
            if (local == gnumeric_border_names[i]) {
                book::border_edge edge;
                edge.line = static_cast<book::border_line>(
                    attr_int(attributes, "Style"));
                read_color(attributes, "Color", edge.color);
                current_.borders[i] = edge;
                return;
            }
        if (local == "HyperLink" || local == "Validation" ||
            local == "Condition" || local == "InputMessage")
            warn_once(std::string(local) + " in styles is not supported");
    }

    void warn_once(std::string message) {
        if (warned_.insert(message).second)
            result_.warnings.push_back(std::move(message));
    }

    void end_region() {
        if (region_.end.col >= sheet_->col_count() ||
            region_.end.row >= sheet_->row_count()) {
            region_.end.col = std::min(region_.end.col, sheet_->col_count() - 1);
            region_.end.row = std::min(region_.end.row, sheet_->row_count() - 1);
        }
        sheet_->styles().set(region_, book_.styles().intern(current_));
    }

    void read_axis(book::axis &target, const xml_attributes &attributes) {
        const int first = attr_int(attributes, "No");
        const int count = attr_int(attributes, "Count", 1);
        const double points = attr_double(attributes, "Unit");
        const bool hard = attr_bool(attributes, "HardSize");
        const bool hidden = attr_bool(attributes, "Hidden");
        for (int i = first; i < first + count && i < target.count(); ++i) {
            target.set_points(i, points, hard);
            if (hidden)
                target.set_hidden(i, true);
        }
    }

    void read_cursor(const xml_attributes &attributes) {
        sheet_->selection.cursor = {attr_int(attributes, "CursorCol"),
                                    attr_int(attributes, "CursorRow")};
    }

    void read_selection(const xml_attributes &attributes) {
        sheet_->selection.ranges.push_back(
            {{attr_int(attributes, "startCol"),
              attr_int(attributes, "startRow")},
             {attr_int(attributes, "endCol"), attr_int(attributes, "endRow")}});
    }

    // Names and attributes share their tag names.
    void end_name_part(std::string_view local) {
        if (local == "name")
            part_name_ = text_;
        else if (local == "value")
            part_value_ = text_;
        else if (local == "position")
            part_position_ = text_;
        else if (local == "Attribute")
            book_.attributes[part_name_] = part_value_;
    }

    void end_defined_name() {
        pending_names_.push_back(
            {sheet_, part_name_, part_value_,
             val::parse_cell_name(part_position_).value_or(cell_pos{})});
        part_position_.clear();
    }

    void define(const pending_name &item) {
        // Built-in print settings live in the preserved XML.
        if (item.name == "Print_Area" || item.name == "Sheet_Title")
            return;
        if (item.value.empty())
            return;
        const expr::parse_pos at{item.scope ? item.scope
                                            : &book_.sheet_at(0),
                                 item.origin, &book_};
        expr::parse_result parsed = expr::parse_formula(item.value, at);
        if (!parsed.tree) {
            result_.warnings.push_back("Name " + item.name +
                                       " has an unreadable definition");
            return;
        }
        book_.define_name(item.name, parsed.tree, item.scope, item.origin);
    }

    // The stored definition of a shared formula.
    struct shared_formula {
        expr::node_ptr formula;
        cell_pos origin;
    };

    book::workbook &book_;
    import_result &result_;
    std::vector<std::string> path_;
    int depth_ = 0;
    std::string text_;
    sheet *sheet_ = nullptr;
    int sheet_index_ = 0;
    int declared_cols_ = val::default_cols;
    int declared_rows_ = val::default_rows;
    pending_cell cell_;
    std::optional<std::string> content_;
    std::vector<pending_cell> pending_;
    std::map<int, shared_formula> shared_;
    cell_range region_;
    style current_;
    std::set<std::string> warned_;
    std::string part_name_;
    std::string part_value_;
    std::string part_position_;
    std::vector<pending_name> pending_names_;
    int preserve_depth_ = 0;
    std::string *preserve_into_ = nullptr;
};

} // namespace

import_result read_gnumeric(std::string_view data, book::workbook &target) {
    import_result result;
    std::optional<std::string> unpacked;
    if (util::is_gzip(data)) {
        unpacked = util::gunzip(data);
        if (!unpacked) {
            result.error = "The compressed file is damaged";
            return result;
        }
        data = *unpacked;
    }
    reader events(target, result);
    std::string error;
    if (!util::parse_xml(data, events, error)) {
        result.error = error;
        return result;
    }
    if (target.sheet_count() == 0) {
        result.error = "The file holds no sheets";
        return result;
    }
    events.finish();
    target.queue_all();
    target.recalculate();
    result.ok = true;
    return result;
}

} // namespace impexp
