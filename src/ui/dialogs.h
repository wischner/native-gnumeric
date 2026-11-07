//
// The small modal dialogs: a one-line question, and the function picker.
// Native has no label control, so each paints its own prompt through
// the theme's panel colors.
//
// Derived from Gnumeric src/dialogs/dialog-formula-guru.c and
// dialog-function-select.c,
// Copyright (C) 2000-2008 Jody Goldberg, Jukka-Pekka Iivonen, Andreas
// J. Guelzow.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#ifndef UI_DIALOGS_H
#define UI_DIALOGS_H

#include <functional>
#include <string>
#include <vector>

#include <native.h>

#include "expr/function.h"

class desktop_test;

namespace ui {

// Asks for one line of text.
class input_window final : public native::modal_wnd {
public:
    // Create the dialog; it stays hidden until asked.
    explicit input_window(native::app_wnd &owner);

    //
    // Ask a question.
    //
    // Parameters:
    //      title   - The window title.
    //      prompt  - The question.
    //      initial - The starting answer.
    //      accept  - Runs with the answer unless the user cancels.
    //
    void ask(const std::string &title, const std::string &prompt,
             const std::string &initial,
             std::function<void(const std::string &)> accept);

private:
    friend class ::desktop_test;

    void attach(native::wnd &child);
    void finish(bool accepted);

    std::string prompt_;
    std::function<void(const std::string &)> accept_;
    native::text_edit answer_;
    native::button ok_;
    native::button cancel_;
};

// Picks a function by category.
class function_window final : public native::modal_wnd {
public:
    // Create the dialog; it stays hidden until asked.
    explicit function_window(native::app_wnd &owner);

    // Ask for a function; accept runs with its name.
    void ask(std::function<void(const std::string &)> accept);

private:
    friend class ::desktop_test;

    void attach(native::wnd &child);
    void show_category();
    void show_summary();
    void finish(bool accepted);

    std::vector<const expr::function_def *> shown_;
    std::function<void(const std::string &)> accept_;
    native::combo_box category_;
    native::list names_;
    native::button ok_;
    native::button cancel_;
    std::string summary_;
};

} // namespace ui

#endif
