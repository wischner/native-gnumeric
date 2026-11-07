//
// Scanner and precedence-climbing parser for formulas.
//
// Derived from Gnumeric src/parser.y and src/parse-util.c,
// Copyright (C) 1998-2002 Miguel de Icaza, Jody Goldberg, Morten Welinder.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include "expr/parser.h"

#include <cctype>
#include <cstdlib>
#include <optional>

#include "util/text.h"

namespace expr {
namespace {

using val::error_code;

// Report whether a byte can start a name.
bool name_start(char c) {
    return std::isalpha(static_cast<unsigned char>(c)) || c == '_' ||
           (static_cast<unsigned char>(c) & 0x80);
}

// Report whether a byte can continue a name.
bool name_part(char c) {
    return name_start(c) || std::isdigit(static_cast<unsigned char>(c)) ||
           c == '.' || c == '?';
}

// One side of a cell reference as written.
struct written_cell {
    int col = 0;
    int row = 0;
    bool col_absolute = false;
    bool row_absolute = false;
    bool has_col = false;
    bool has_row = false;
};

// The parser for one formula.
class parser {
public:
    parser(std::string_view text, const parse_pos &pos)
        : text_(text), pos_(pos) {}

    // Parse the entire text.
    parse_result run() {
        parse_result result;
        spaces();
        if (peek() == '=')
            ++at_;
        node_ptr tree = comparison();
        spaces();
        if (!failed_ && at_ < text_.size())
            fail("Unexpected character");
        if (failed_) {
            result.error = error_;
            result.error_at = error_at_;
            return result;
        }
        result.tree = std::move(tree);
        return result;
    }

private:
    char peek(std::size_t ahead = 0) const {
        return at_ + ahead < text_.size() ? text_[at_ + ahead] : '\0';
    }

    void spaces() {
        while (peek() == ' ' || peek() == '\t' || peek() == '\n' ||
               peek() == '\r')
            ++at_;
    }

    // Record the first failure and return no tree.
    node_ptr fail(const char *message) {
        if (!failed_) {
            failed_ = true;
            error_ = message;
            error_at_ = at_;
        }
        return nullptr;
    }

    // Consume a character after spaces when it is next.
    bool accept(char c) {
        spaces();
        if (peek() != c)
            return false;
        ++at_;
        return true;
    }

    // Parse the lowest level: comparisons.
    node_ptr comparison() {
        node_ptr left = concatenation();
        while (!failed_) {
            spaces();
            binary_op op;
            std::size_t length = 1;
            const char c = peek();
            const char next = peek(1);
            if (c == '<' && next == '=') {
                op = binary_op::le;
                length = 2;
            } else if (c == '>' && next == '=') {
                op = binary_op::ge;
                length = 2;
            } else if (c == '<' && next == '>') {
                op = binary_op::ne;
                length = 2;
            } else if (c == '<') {
                op = binary_op::lt;
            } else if (c == '>') {
                op = binary_op::gt;
            } else if (c == '=') {
                op = binary_op::eq;
            } else {
                break;
            }
            at_ += length;
            node_ptr right = concatenation();
            if (failed_)
                return nullptr;
            left = make_binary(op, std::move(left), std::move(right));
        }
        return left;
    }

    node_ptr concatenation() {
        node_ptr left = additive();
        while (!failed_ && accept('&')) {
            node_ptr right = additive();
            if (failed_)
                return nullptr;
            left = make_binary(binary_op::concat, std::move(left),
                               std::move(right));
        }
        return left;
    }

    node_ptr additive() {
        node_ptr left = multiplicative();
        while (!failed_) {
            spaces();
            const char c = peek();
            if (c != '+' && c != '-')
                break;
            ++at_;
            node_ptr right = multiplicative();
            if (failed_)
                return nullptr;
            left = make_binary(c == '+' ? binary_op::add : binary_op::sub,
                               std::move(left), std::move(right));
        }
        return left;
    }

    node_ptr multiplicative() {
        node_ptr left = power();
        while (!failed_) {
            spaces();
            const char c = peek();
            if (c != '*' && c != '/')
                break;
            ++at_;
            node_ptr right = power();
            if (failed_)
                return nullptr;
            left = make_binary(c == '*' ? binary_op::mul : binary_op::div,
                               std::move(left), std::move(right));
        }
        return left;
    }

    // Exponentiation is right associative, and gets parentheses around
    // chains as Gnumeric prints them.
    node_ptr power() {
        node_ptr left = postfix();
        if (failed_ || !accept('^'))
            return left;
        node_ptr right = power();
        if (failed_)
            return nullptr;
        return make_binary(binary_op::pow, std::move(left), std::move(right));
    }

    node_ptr postfix() {
        node_ptr operand = unary();
        while (!failed_) {
            spaces();
            if (peek() != '%')
                break;
            ++at_;
            operand = make_unary(unary_op::percent, std::move(operand));
        }
        return operand;
    }

    node_ptr unary() {
        spaces();
        if (peek() == '-' || peek() == '+') {
            const bool negative = peek() == '-';
            ++at_;
            node_ptr operand = unary();
            if (failed_)
                return nullptr;
            if (negative) {
                if (const value *constant = operand->as_constant();
                    constant && constant->is_number())
                    return make_constant(-constant->number());
                return make_unary(unary_op::neg, std::move(operand));
            }
            return make_unary(unary_op::plus, std::move(operand));
        }
        return intersection();
    }

    // Report whether a reference may begin at the cursor.
    bool reference_follows() const {
        const char c = peek();
        return c == '$' || c == '\'' || name_start(c) ||
               std::isdigit(static_cast<unsigned char>(c));
    }

    // A space between references intersects them.
    node_ptr intersection() {
        node_ptr left = primary();
        while (!failed_ && left && is_reference(*left)) {
            const std::size_t before = at_;
            spaces();
            if (at_ == before || !reference_follows())
                break;
            const std::size_t mark = at_;
            auto saved = std::make_pair(failed_, error_);
            node_ptr right = primary();
            if (failed_ || !right || !is_reference(*right)) {
                failed_ = saved.first;
                error_ = saved.second;
                at_ = mark;
                break;
            }
            left = make_intersection(std::move(left), std::move(right));
        }
        return left;
    }

    // Report whether a node denotes cells.
    static bool is_reference(const node &item) {
        const eval_pos probe{};
        return item.as_area(probe).has_value();
    }

    node_ptr primary() {
        spaces();
        const char c = peek();
        if (c == '(')
            return parenthesized();
        if (c == '{')
            return array_literal();
        if (c == '"')
            return string_literal();
        if (c == '#')
            return error_literal();
        if (c == '\'' || c == '[')
            return qualified();
        if (c == '$')
            return reference(pos_.sheet, true);
        if (std::isdigit(static_cast<unsigned char>(c)) || c == '.') {
            if (node_ptr rows = row_range(nullptr))
                return rows;
            return number_literal();
        }
        if (name_start(c))
            return word();
        return fail("Expression expected");
    }

    node_ptr parenthesized() {
        ++at_;
        std::vector<node_ptr> members;
        do {
            node_ptr member = comparison();
            if (failed_)
                return nullptr;
            members.push_back(std::move(member));
        } while (accept(','));
        if (!accept(')'))
            return fail("Missing )");
        if (members.size() == 1)
            return make_unary(unary_op::paren, std::move(members[0]));
        return make_set(std::move(members));
    }

    node_ptr number_literal() {
        const std::size_t start = at_;
        while (std::isdigit(static_cast<unsigned char>(peek())))
            ++at_;
        if (peek() == '.') {
            ++at_;
            while (std::isdigit(static_cast<unsigned char>(peek())))
                ++at_;
        }
        if (peek() == 'e' || peek() == 'E') {
            std::size_t ahead = 1;
            if (peek(1) == '+' || peek(1) == '-')
                ahead = 2;
            if (std::isdigit(static_cast<unsigned char>(peek(ahead)))) {
                at_ += ahead;
                while (std::isdigit(static_cast<unsigned char>(peek())))
                    ++at_;
            }
        }
        const std::string digits(text_.substr(start, at_ - start));
        if (digits == ".")
            return fail("Number expected");
        return make_constant(std::strtod(digits.c_str(), nullptr));
    }

    // Scan a quoted string, with both \" and "" escapes.
    std::string quoted(char quote) {
        ++at_;
        std::string out;
        while (at_ < text_.size()) {
            const char c = text_[at_];
            if (c == '\\' && at_ + 1 < text_.size()) {
                out.push_back(text_[at_ + 1]);
                at_ += 2;
            } else if (c == quote && peek(1) == quote) {
                out.push_back(quote);
                at_ += 2;
            } else if (c == quote) {
                ++at_;
                return out;
            } else {
                out.push_back(c);
                ++at_;
            }
        }
        fail("Unterminated string");
        return out;
    }

    node_ptr string_literal() {
        std::string text = quoted('"');
        if (failed_)
            return nullptr;
        return make_constant(value(text));
    }

    node_ptr error_literal() {
        std::size_t end = at_ + 1;
        while (end < text_.size() &&
               (name_part(text_[end]) || text_[end] == '/' ||
                text_[end] == '!' || text_[end] == '0'))
            ++end;
        for (std::size_t length = end - at_; length > 1; --length) {
            if (const auto code = val::parse_error(text_.substr(at_, length))) {
                at_ += length;
                return make_constant(value(*code));
            }
        }
        return fail("Unknown error constant");
    }

    node_ptr array_literal() {
        ++at_;
        std::vector<std::vector<value>> rows(1);
        while (true) {
            spaces();
            node_ptr element = unary();
            if (failed_)
                return nullptr;
            const value *constant = element->as_constant();
            if (!constant)
                return fail("Array elements must be constants");
            rows.back().push_back(*constant);
            if (accept(','))
                continue;
            if (accept(';')) {
                rows.emplace_back();
                continue;
            }
            if (accept('}'))
                break;
            return fail("Missing }");
        }
        const std::size_t cols = rows[0].size();
        std::vector<value> elements;
        for (auto &row : rows) {
            if (row.size() != cols)
                return fail("Array rows differ in length");
            for (value &element : row)
                elements.push_back(std::move(element));
        }
        return make_constant(value(std::make_shared<const val::array_value>(
            static_cast<int>(cols), static_cast<int>(rows.size()),
            std::move(elements))));
    }

    // Scan $A$1, A1, $A or 1 at the cursor.
    std::optional<written_cell> scan_cell() {
        const std::size_t start = at_;
        written_cell cell;
        cell.col_absolute = peek() == '$';
        if (cell.col_absolute)
            ++at_;
        const std::size_t letters = at_;
        while (std::isalpha(static_cast<unsigned char>(peek())))
            ++at_;
        if (at_ > letters) {
            const auto col = val::parse_col(text_.substr(letters, at_ - letters));
            if (!col || *col >= sheet_cols()) {
                at_ = start;
                return std::nullopt;
            }
            cell.col = *col;
            cell.has_col = true;
        }
        cell.row_absolute = peek() == '$';
        if (cell.row_absolute)
            ++at_;
        const std::size_t digits = at_;
        while (std::isdigit(static_cast<unsigned char>(peek())))
            ++at_;
        if (at_ > digits && at_ - digits <= 9) {
            const int row = std::atoi(
                std::string(text_.substr(digits, at_ - digits)).c_str());
            if (row < 1 || row > sheet_rows()) {
                at_ = start;
                return std::nullopt;
            }
            cell.row = row - 1;
            cell.has_row = true;
        }
        if (!cell.has_col && !cell.has_row) {
            at_ = start;
            return std::nullopt;
        }
        return cell;
    }

    int sheet_cols() const {
        return pos_.sheet ? pos_.sheet->col_count() : val::default_cols;
    }

    int sheet_rows() const {
        return pos_.sheet ? pos_.sheet->row_count() : val::default_rows;
    }

    // Turn a written corner into a reference for this formula.
    cell_ref to_ref(const written_cell &cell, cell_source *sheet) const {
        return cell_ref::make({cell.col, cell.row}, pos_.pos,
                              !cell.col_absolute, !cell.row_absolute,
                              sheet == pos_.sheet ? nullptr : sheet);
    }

    // Build a reference from one or two scanned corners.
    node_ptr build_reference(const written_cell &first,
                             const std::optional<written_cell> &second,
                             cell_source *sheet) {
        if (!second) {
            if (!first.has_col || !first.has_row)
                return fail("Incomplete reference");
            return make_cell(to_ref(first, sheet));
        }
        written_cell a = first;
        written_cell b = *second;
        if (a.has_col != b.has_col || a.has_row != b.has_row)
            return fail("Mismatched range corners");
        if (!a.has_row) {
            a.row = 0;
            b.row = sheet_rows() - 1;
            a.row_absolute = b.row_absolute = true;
        }
        if (!a.has_col) {
            a.col = 0;
            b.col = sheet_cols() - 1;
            a.col_absolute = b.col_absolute = true;
        }
        return make_range(to_ref(a, sheet), to_ref(b, sheet));
    }

    // Parse a reference at the cursor; a failure is an error when told.
    node_ptr reference(cell_source *sheet, bool required) {
        const std::size_t start = at_;
        const auto first = scan_cell();
        if (!first) {
            if (required)
                return fail("Invalid reference");
            return nullptr;
        }
        std::optional<written_cell> second;
        if (peek() == ':') {
            const std::size_t colon = at_;
            ++at_;
            // Sheet!A1:Sheet!B2 repeats the sheet on the far corner.
            spaces();
            second = scan_cell();
            if (!second)
                at_ = colon;
        }
        // IF( and T1( are calls, and a lone column is a name, not a cell.
        const bool incomplete =
            !second && (!first->has_col || !first->has_row);
        if (!required && (name_part(peek()) || peek() == '(' || incomplete)) {
            at_ = start;
            return nullptr;
        }
        return build_reference(*first, second, sheet);
    }

    // Parse 1:3, a range of whole rows.
    node_ptr row_range(cell_source *sheet) {
        const std::size_t start = at_;
        std::size_t end = at_;
        while (end < text_.size() &&
               std::isdigit(static_cast<unsigned char>(text_[end])))
            ++end;
        if (end == at_ || end >= text_.size() || text_[end] != ':')
            return nullptr;
        const auto first = scan_cell();
        if (!first || peek() != ':') {
            at_ = start;
            return nullptr;
        }
        ++at_;
        const auto second = scan_cell();
        if (!second || second->has_col) {
            at_ = start;
            return nullptr;
        }
        return build_reference(*first, second,
                               sheet ? sheet : pos_.sheet);
    }

    // Resolve a sheet name; an unknown one yields #REF!.
    node_ptr after_sheet(std::string_view name) {
        ++at_; // The '!'.
        cell_source *sheet =
            pos_.scope ? pos_.scope->find_sheet(name) : nullptr;
        const std::size_t mark = at_;
        if (sheet) {
            if (node_ptr rows = row_range(sheet))
                return rows;
            if (node_ptr cells = reference(sheet, false))
                return cells;
            at_ = mark;
            std::string name_text = scan_name();
            if (!name_text.empty() && pos_.scope)
                return make_name(
                    pos_.scope->find_name(name_text, sheet, true));
            return fail("Invalid reference after sheet name");
        }
        // Consume what the unknown sheet was qualifying.
        const auto first = scan_cell();
        if (first) {
            if (peek() == ':') {
                ++at_;
                scan_cell();
            }
        } else {
            scan_name();
        }
        return make_constant(value(error_code::ref));
    }

    // Parse a form that opens with a quoted or bracketed qualifier.
    node_ptr qualified() {
        if (peek() == '[') {
            const auto close = text_.find(']', at_);
            if (close == std::string_view::npos)
                return fail("Missing ]");
            at_ = close + 1;
            if (peek() == '\'') {
                quoted('\'');
            } else {
                scan_name();
            }
            if (failed_ || peek() != '!')
                return fail("Missing ! after workbook");
            return after_sheet("");
        }
        std::string name = quoted('\'');
        if (failed_)
            return nullptr;
        if (peek() == ':') {
            // Sheet1:Sheet3!A1, a 3-D range, is not supported yet.
            return fail("3-D references are not supported");
        }
        if (peek() != '!')
            return fail("Missing ! after sheet name");
        return after_sheet(name);
    }

    std::string scan_name() {
        const std::size_t start = at_;
        while (name_part(peek()))
            ++at_;
        return std::string(text_.substr(start, at_ - start));
    }

    // Parse what starts with a letter.
    node_ptr word() {
        if (node_ptr cells = reference(pos_.sheet, false))
            return cells;
        std::string name = scan_name();
        spaces();
        if (peek() == '(')
            return call(std::move(name));
        if (peek() == '!')
            return after_sheet(name);
        if (peek() == ':' && !pos_.scope)
            return fail("3-D references are not supported");
        if (const auto flag = val::parse_boolean(name))
            return make_constant(value(*flag));
        if (!pos_.scope)
            return make_constant(value(error_code::name));
        return make_name(pos_.scope->find_name(name, pos_.sheet, true));
    }

    node_ptr call(std::string name) {
        ++at_;
        std::vector<node_ptr> arguments;
        spaces();
        if (peek() == ')') {
            ++at_;
        } else {
            while (true) {
                spaces();
                if (peek() == ',' || peek() == ')') {
                    arguments.push_back(make_constant(value()));
                } else {
                    node_ptr argument = comparison();
                    if (failed_)
                        return nullptr;
                    arguments.push_back(std::move(argument));
                }
                if (accept(','))
                    continue;
                if (accept(')'))
                    break;
                return fail("Missing ) in function call");
            }
        }
        const function_def *function =
            pos_.scope ? pos_.scope->find_function(name) : nullptr;
        if (function)
            return make_call(function, std::move(arguments));
        return make_unknown_call(std::move(name), std::move(arguments));
    }

    std::string_view text_;
    const parse_pos &pos_;
    std::size_t at_ = 0;
    bool failed_ = false;
    std::string error_;
    std::size_t error_at_ = 0;
};

} // namespace

parse_result parse_formula(std::string_view text, const parse_pos &pos) {
    return parser(text, pos).run();
}

std::string quote_sheet_name(std::string_view name) {
    bool plain = !name.empty() && name_start(name[0]);
    for (const char c : name)
        plain = plain && name_part(c) && c != '?';
    // Sheet1 spells column SHEET, far beyond any real sheet.
    if (const auto cell = val::parse_cell_name(name);
        plain && cell && cell->col < (1 << 20))
        plain = false;
    if (plain)
        return std::string(name);
    std::string out = "'";
    for (const char c : name) {
        if (c == '\'' || c == '\\')
            out.push_back('\\');
        out.push_back(c);
    }
    out.push_back('\'');
    return out;
}

} // namespace expr
