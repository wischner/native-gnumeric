//
// Undoable edits. Each user action is one command object that knows how
// to do and undo itself, as Gnumeric's GnmCommand classes do; the stack
// owns the history and keeps the file's modified state. Most commands
// are mementos of the rectangle they touch, so they are short and undo
// cannot drift from do.
//
// Derived from Gnumeric src/commands.c,
// Copyright (C) 1999-2008 Jody Goldberg, 2002-2009 Morten Welinder.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#ifndef CMD_COMMAND_H
#define CMD_COMMAND_H

#include <memory>
#include <string>
#include <vector>

#include "book/workbook.h"

namespace cmd {

// One undoable action.
class command {
public:
    virtual ~command() = default;

    // Apply the action.
    virtual void redo(book::workbook &target) = 0;

    // Reverse the action.
    virtual void undo(book::workbook &target) = 0;

    // Return what the menus call this action.
    virtual std::string describe() const = 0;
};

// The history of a workbook.
class command_stack {
public:
    // Create a stack over a workbook.
    explicit command_stack(book::workbook &target);

    //
    // Run a command and remember it.
    //
    // Notes:
    //      The redo history goes, and the workbook recalculates.
    //
    void run(std::unique_ptr<command> action);

    // Report whether an action can be undone.
    bool can_undo() const { return !undo_.empty(); }

    // Report whether an action can be redone.
    bool can_redo() const { return !redo_.empty(); }

    // Undo the latest action.
    void undo();

    // Redo the latest undone action.
    void redo();

    // Return the description of the next undo; empty when none.
    std::string undo_name() const;

    // Return the description of the next redo; empty when none.
    std::string redo_name() const;

    // Report whether the workbook differs from its saved state.
    bool modified() const { return modified_; }

    // Record that the workbook was saved.
    void mark_saved();

    // Forget all history.
    void clear();

    // Set the most actions kept.
    void set_limit(std::size_t limit) { limit_ = limit; }

private:
    using action_ptr = std::unique_ptr<command>;

    book::workbook *target_;
    std::vector<action_ptr> undo_;
    std::vector<action_ptr> redo_;
    std::size_t limit_ = 500;
    bool modified_ = false;

    // The undo depth at saving; negative when that state is gone.
    long saved_depth_ = 0;
};

} // namespace cmd

#endif
