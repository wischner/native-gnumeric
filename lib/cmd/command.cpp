//
// The undo stack.
//
// Derived from Gnumeric src/commands.c,
// Copyright (C) 1999-2008 Jody Goldberg, 2002-2009 Morten Welinder.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include "cmd/command.h"

namespace cmd {

command_stack::command_stack(book::workbook &target) : target_(&target) {}

void command_stack::run(std::unique_ptr<command> action) {
    action->redo(*target_);
    target_->recalculate();
    if (saved_depth_ > static_cast<long>(undo_.size()))
        saved_depth_ = -1;
    undo_.push_back(std::move(action));
    redo_.clear();
    if (undo_.size() > limit_) {
        undo_.erase(undo_.begin());
        --saved_depth_;
    }
    modified_ = true;
}

void command_stack::undo() {
    if (undo_.empty())
        return;
    action_ptr action = std::move(undo_.back());
    undo_.pop_back();
    action->undo(*target_);
    target_->recalculate();
    redo_.push_back(std::move(action));
    modified_ = static_cast<long>(undo_.size()) != saved_depth_;
}

void command_stack::redo() {
    if (redo_.empty())
        return;
    action_ptr action = std::move(redo_.back());
    redo_.pop_back();
    action->redo(*target_);
    target_->recalculate();
    undo_.push_back(std::move(action));
    modified_ = static_cast<long>(undo_.size()) != saved_depth_;
}

std::string command_stack::undo_name() const {
    return undo_.empty() ? std::string() : undo_.back()->describe();
}

std::string command_stack::redo_name() const {
    return redo_.empty() ? std::string() : redo_.back()->describe();
}

void command_stack::mark_saved() {
    saved_depth_ = static_cast<long>(undo_.size());
    modified_ = false;
}

void command_stack::clear() {
    undo_.clear();
    redo_.clear();
    saved_depth_ = 0;
    modified_ = false;
}

} // namespace cmd
