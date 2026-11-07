//
// The text being edited in a cell. One model feeds the in-cell editor
// and the formula bar, so the two cannot disagree. It also tracks the
// reference being pointed at: while a formula is typed, moving the
// cursor over the grid writes a reference at the caret and keeps
// replacing it as the pointer moves.
//
// Derived from Gnumeric src/item-edit.c and src/widgets/gnm-expr-entry.c,
// Copyright (C) 1998-2001 Miguel de Icaza, Jody Goldberg.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#ifndef VIEW_EDITOR_H
#define VIEW_EDITOR_H

#include <string>
#include <string_view>

namespace view {

// An editable line of text with a caret and a selection.
class cell_editor {
public:
    // Report whether an edit is open.
    bool active() const { return active_; }

    // Open an edit over some text, caret at the end.
    void open(std::string_view text);

    // Close the edit.
    void close();

    // Return the text as UTF-8.
    std::string text() const;

    // Return the text as code points.
    const std::u32string &characters() const { return text_; }

    // Return the caret, in characters.
    std::size_t caret() const { return caret_; }

    // Return the selection anchor, in characters.
    std::size_t anchor() const { return anchor_; }

    // Report whether text is selected.
    bool has_selection() const { return anchor_ != caret_; }

    // Replace the selection with text.
    void insert(std::string_view utf8);

    // Remove the selection or the character before the caret.
    void erase_left();

    // Remove the selection or the character after the caret.
    void erase_right();

    // Move the caret by characters, optionally extending the selection.
    void move(int delta, bool extend);

    // Move the caret to a place, optionally extending the selection.
    void set_caret(std::size_t place, bool extend);

    // Select everything.
    void select_all();

    // Report whether the text is a formula.
    bool is_formula() const;

    //
    // Report whether a reference can be pointed at.
    //
    // Returns:
    //      True at the start of an operand of a formula: just after an
    //      operator, or inside a reference already being pointed at.
    //
    bool can_point() const;

    // Replace the pointed reference with text, or start one at the caret.
    void set_reference(std::string_view text);

    // Report whether a reference is being pointed at.
    bool pointing() const { return pointing_; }

    // Stop pointing; the reference stays in the text.
    void end_pointing() { pointing_ = false; }

private:
    // Remove the selection, leaving the caret where it was.
    void erase_selection();

    std::u32string text_;
    std::size_t caret_ = 0;
    std::size_t anchor_ = 0;
    bool active_ = false;
    bool pointing_ = false;
    std::size_t reference_start_ = 0;
    std::size_t reference_end_ = 0;
};

} // namespace view

#endif
