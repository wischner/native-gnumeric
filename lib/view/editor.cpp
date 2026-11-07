//
// The edit line.
//
// Derived from Gnumeric src/item-edit.c and src/widgets/gnm-expr-entry.c,
// Copyright (C) 1998-2001 Miguel de Icaza, Jody Goldberg.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include "view/editor.h"

#include <algorithm>

#include "util/utf8.h"

namespace view {

void cell_editor::open(std::string_view text) {
    text_ = util::to_utf32(text);
    caret_ = anchor_ = text_.size();
    active_ = true;
    pointing_ = false;
}

void cell_editor::close() {
    active_ = false;
    pointing_ = false;
    text_.clear();
    caret_ = anchor_ = 0;
}

std::string cell_editor::text() const {
    return util::to_utf8(text_);
}

void cell_editor::erase_selection() {
    const std::size_t from = std::min(caret_, anchor_);
    const std::size_t to = std::max(caret_, anchor_);
    text_.erase(from, to - from);
    caret_ = anchor_ = from;
}

void cell_editor::insert(std::string_view utf8) {
    pointing_ = false;
    erase_selection();
    const std::u32string added = util::to_utf32(utf8);
    text_.insert(caret_, added);
    caret_ += added.size();
    anchor_ = caret_;
}

void cell_editor::erase_left() {
    pointing_ = false;
    if (has_selection()) {
        erase_selection();
    } else if (caret_ > 0) {
        text_.erase(--caret_, 1);
        anchor_ = caret_;
    }
}

void cell_editor::erase_right() {
    pointing_ = false;
    if (has_selection()) {
        erase_selection();
    } else if (caret_ < text_.size()) {
        text_.erase(caret_, 1);
    }
}

void cell_editor::move(int delta, bool extend) {
    pointing_ = false;
    long place = static_cast<long>(caret_) + delta;
    place = std::clamp(place, 0L, static_cast<long>(text_.size()));
    caret_ = static_cast<std::size_t>(place);
    if (!extend)
        anchor_ = caret_;
}

void cell_editor::set_caret(std::size_t place, bool extend) {
    pointing_ = false;
    caret_ = std::min(place, text_.size());
    if (!extend)
        anchor_ = caret_;
}

void cell_editor::select_all() {
    anchor_ = 0;
    caret_ = text_.size();
}

bool cell_editor::is_formula() const {
    return !text_.empty() &&
           (text_[0] == U'=' || text_[0] == U'+' || text_[0] == U'-');
}

bool cell_editor::can_point() const {
    if (!active_ || !is_formula())
        return false;
    if (pointing_)
        return true;
    if (caret_ == 0 || caret_ != text_.size())
        return false;
    // An operand can begin after an operator or an opening mark.
    static constexpr std::u32string_view openers = U"=+-*/^&<>(,;%";
    return openers.find(text_[caret_ - 1]) != std::u32string_view::npos;
}

void cell_editor::set_reference(std::string_view text) {
    if (!pointing_) {
        reference_start_ = reference_end_ = caret_;
        pointing_ = true;
    }
    const std::u32string added = util::to_utf32(text);
    text_.replace(reference_start_, reference_end_ - reference_start_, added);
    reference_end_ = reference_start_ + added.size();
    caret_ = anchor_ = reference_end_;
}

} // namespace view
