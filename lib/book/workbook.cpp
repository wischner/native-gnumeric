//
// Names, sheets and the calculation walk.
//
// Derived from Gnumeric src/workbook.c and src/dependent.c,
// Copyright (C) 2000-2006 Jody Goldberg, 1998-2002 Michael Meeks,
// 2000-2018 Morten Welinder.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include "book/workbook.h"

#include <algorithm>
#include <cmath>

#include "func/functions.h"
#include "util/text.h"

namespace book {
namespace {

// Records what a formula reads.
class read_collector final : public expr::dependency_sink {
public:
    read_collector(std::vector<precedent> &reads, bool &is_volatile)
        : reads_(reads), volatile_(is_volatile) {}

    void depends_on(val::cell_source *sheet, cell_range range) override {
        const precedent item{sheet, range};
        for (const precedent &existing : reads_)
            if (existing.sheet == sheet && existing.range == range)
                return;
        reads_.push_back(item);
    }

    void depends_on_time() override { volatile_ = true; }

private:
    std::vector<precedent> &reads_;
    bool &volatile_;
};

// Return how far two values are apart, for iteration.
double difference(const value &a, const value &b) {
    if (a.is_number() && b.is_number())
        return std::fabs(a.number() - b.number());
    return a == b ? 0 : 1;
}

} // namespace

workbook::workbook() : functions_(&func::builtin_functions()) {}

workbook::~workbook() {
    dependents_.clear();
    sheets_.clear();
}

expr::cell_source *workbook::find_sheet(std::string_view name) {
    for (const auto &item : sheets_)
        if (util::equal_nocase(item->name(), name))
            return item.get();
    return nullptr;
}

const expr::function_def *workbook::find_function(std::string_view name) {
    return functions_->find(name);
}

std::shared_ptr<expr::named_expr>
workbook::find_name(std::string_view name, expr::cell_source *from,
                    bool create) {
    std::shared_ptr<expr::named_expr> global;
    for (const auto &item : names_) {
        if (!util::equal_nocase(item->name, name))
            continue;
        if (item->sheet == from && from)
            return item;
        if (!item->sheet)
            global = item;
    }
    if (global || !create)
        return global;
    auto placeholder = std::make_shared<expr::named_expr>();
    placeholder->name = name;
    names_.push_back(placeholder);
    return placeholder;
}

sheet &workbook::add_sheet(std::string name, int cols, int rows) {
    return insert_sheet(sheet_count(), std::move(name), cols, rows);
}

sheet &workbook::insert_sheet(int index, std::string name, int cols,
                              int rows) {
    auto created = std::make_unique<sheet>(*this, std::move(name), cols, rows);
    sheet &result = *created;
    sheets_.insert(sheets_.begin() + index, std::move(created));
    return result;
}

bool workbook::remove_sheet(int index) {
    if (sheet_count() <= 1)
        return false;
    take_sheet(index);
    return true;
}

removed_sheet workbook::take_sheet(int index) {
    removed_sheet removed;
    sheet *doomed = sheets_[static_cast<std::size_t>(index)].get();
    // Formulas elsewhere stop referring to it.
    class deleting final : public expr::ref_mapper {
    public:
        explicit deleting(sheet *target) : target_(target) {}
        std::optional<cell_range> map_range(val::cell_source *,
                                            cell_range range) override {
            return range;
        }
        cell_pos map_origin(cell_pos pos) override { return pos; }
        bool sheet_deleted(val::cell_source *item) override {
            return item == target_;
        }

    private:
        sheet *target_;
    } mapper(doomed);
    removed.record = rewrite_formulas(mapper, doomed);
    for (const auto &name : names_)
        if (name->sheet == doomed)
            removed.names.push_back(name);
    std::erase_if(names_, [&](const auto &name) {
        return name->sheet == doomed;
    });
    removed.item = std::move(sheets_[static_cast<std::size_t>(index)]);
    sheets_.erase(sheets_.begin() + index);
    rebuild_dependencies();
    queue_all();
    recalculate();
    return removed;
}

void workbook::restore_sheet(int index, removed_sheet removed) {
    sheets_.insert(sheets_.begin() + index, std::move(removed.item));
    names_.insert(names_.end(), removed.names.begin(), removed.names.end());
    restore_formulas(removed.record);
    rebuild_dependencies();
    queue_all();
}

void workbook::move_sheet(int from, int to) {
    auto moved = std::move(sheets_[static_cast<std::size_t>(from)]);
    sheets_.erase(sheets_.begin() + from);
    sheets_.insert(sheets_.begin() + to, std::move(moved));
}

int workbook::index_of(const sheet &item) const {
    for (int i = 0; i < sheet_count(); ++i)
        if (&sheet_at(i) == &item)
            return i;
    return -1;
}

std::string workbook::unique_sheet_name() const {
    for (int i = 1;; ++i) {
        const std::string name = "Sheet" + std::to_string(i);
        bool taken = false;
        for (const auto &item : sheets_)
            taken = taken || util::equal_nocase(item->name(), name);
        if (!taken)
            return name;
    }
}

std::shared_ptr<expr::named_expr>
workbook::define_name(std::string_view name, expr::node_ptr definition,
                      sheet *scope, cell_pos origin) {
    std::shared_ptr<expr::named_expr> entry;
    for (const auto &item : names_)
        if (util::equal_nocase(item->name, name) && item->sheet == scope)
            entry = item;
    if (!entry)
        for (const auto &item : names_)
            if (util::equal_nocase(item->name, name) && !item->definition)
                entry = item;
    if (!entry) {
        entry = std::make_shared<expr::named_expr>();
        entry->name = name;
        names_.push_back(entry);
    }
    entry->sheet = scope;
    entry->definition = std::move(definition);
    entry->origin = origin;
    rebuild_dependencies();
    queue_all();
    return entry;
}

void workbook::undefine_name(const std::shared_ptr<expr::named_expr> &name) {
    name->definition.reset();
    rebuild_dependencies();
    queue_all();
}

void workbook::attach(cell &formula) {
    bool is_volatile = false;
    formula.reads.clear();
    read_collector sink(formula.reads, is_volatile);
    if (formula.formula)
        formula.formula->collect(sink, &formula.owner(), formula.pos());
    formula.is_volatile = is_volatile;
    if (is_volatile)
        volatile_.insert(&formula);
    dependents_.add(formula);
}

void workbook::detach(cell &formula) {
    dependents_.remove(formula);
    formula.reads.clear();
}

void workbook::queue_readers(sheet &where, cell_pos pos) {
    std::vector<std::pair<sheet *, cell_pos>> pending{{&where, pos}};
    const auto mark = [&](cell &reader) {
        if (reader.dirty)
            return;
        reader.dirty = true;
        reader.owner().set_dirty_mark(reader.pos(), true);
        pending.emplace_back(&reader.owner(), reader.pos());
    };
    while (!pending.empty()) {
        const auto [owner, place] = pending.back();
        pending.pop_back();
        dependents_.for_each_reader(owner, place, mark);
    }
}

void workbook::queue_cell(cell &item) {
    if (item.formula && !item.dirty) {
        item.dirty = true;
        item.owner().set_dirty_mark(item.pos(), true);
    }
    queue_readers(item.owner(), item.pos());
}

void workbook::queue_all() {
    for (const auto &item : sheets_)
        item->for_each_stored([&](cell &formula) {
            if (formula.has_formula() && !formula.dirty) {
                formula.dirty = true;
                formula.owner().set_dirty_mark(formula.pos(), true);
            }
        });
}

void workbook::evaluate(cell &item) {
    if (item.array.cols > 1 || item.array.rows > 1 || item.in_array) {
        evaluate_array(item);
        return;
    }
    const expr::eval_pos where{&item.owner(), item.pos(), this, false};
    value result = item.formula->eval(where, expr::eval_mode::scalar);
    if (result.is_grid())
        result = expr::intersect_scalar(result, where);
    item.result = std::move(result);
}

void workbook::evaluate_array(cell &item) {
    const expr::eval_pos where{&item.owner(), item.pos(), this, true};
    const value result = item.formula->eval(where, expr::eval_mode::any);
    const array_part part = item.array;
    for (int row = 0; row < part.rows; ++row)
        for (int col = 0; col < part.cols; ++col) {
            cell *element = item.owner().find(
                {part.corner.col + col, part.corner.row + row});
            if (!element)
                continue;
            const int cols = result.grid_cols();
            const int rows = result.grid_rows();
            const int x = cols == 1 ? 0 : col;
            const int y = rows == 1 ? 0 : row;
            element->result = x < cols && y < rows
                                  ? result.grid_at(x, y)
                                  : value(val::error_code::na);
        }
}

void workbook::calculate(cell &root) {
    std::vector<cell *> stack{&root};
    const bool outermost = !in_top_level_;
    in_top_level_ = true;
    while (!stack.empty()) {
        cell *top = stack.back();
        if (!top->dirty) {
            top->being_calculated = false;
            stack.pop_back();
            continue;
        }
        top->being_calculated = true;
        cell *next = nullptr;
        for (const precedent &read : top->reads) {
            auto &target = static_cast<sheet &>(*read.sheet);
            next = target.first_dirty(read.range);
            if (next)
                break;
        }
        if (!next) {
            // An input still on the stack closes a cycle: everything
            // above it on the stack is a member.
            for (const precedent &read : top->reads) {
                auto &target = static_cast<sheet &>(*read.sheet);
                target.for_each_dirty(read.range, [&](cell &closing) {
                    if (!closing.being_calculated)
                        return;
                    const auto start =
                        std::find(stack.begin(), stack.end(), &closing);
                    for (auto member = start; member != stack.end(); ++member)
                        if (!(*member)->in_cycle) {
                            (*member)->in_cycle = true;
                            cycle_.push_back(*member);
                        }
                });
            }
            if (top->formula)
                evaluate(*top);
            top->dirty = false;
            top->owner().set_dirty_mark(top->pos(), false);
            top->being_calculated = false;
            stack.pop_back();
            continue;
        }
        stack.push_back(next);
    }
    if (outermost) {
        in_top_level_ = false;
        if (!cycle_.empty())
            iterate_cycle();
    }
}

void workbook::iterate_cycle() {
    std::vector<cell *> members;
    members.swap(cycle_);
    for (cell *member : members)
        member->in_cycle = false;
    if (!iteration_enabled)
        return;
    const std::set<cell *> group(members.begin(), members.end());
    for (int round = 0; round < iteration_limit; ++round) {
        double change = 0;
        for (cell *member : members) {
            if (!member->formula)
                continue;
            const value before = member->result;
            evaluate(*member);
            change = std::max(change, difference(before, member->result));
        }
        if (change < iteration_tolerance)
            break;
    }
    // Whatever read a member while it was unsettled is stale.
    std::vector<cell *> pending(members);
    while (!pending.empty()) {
        cell *source = pending.back();
        pending.pop_back();
        dependents_.for_each_reader(
            &source->owner(), source->pos(), [&](cell &reader) {
                if (group.count(&reader) || reader.dirty)
                    return;
                reader.dirty = true;
                reader.owner().set_dirty_mark(reader.pos(), true);
                pending.push_back(&reader);
            });
    }
}

void workbook::recalculate() {
    for (cell *item : volatile_) {
        if (!item->dirty) {
            item->dirty = true;
            item->owner().set_dirty_mark(item->pos(), true);
        }
        queue_readers(item->owner(), item->pos());
    }
    for (const auto &item : sheets_) {
        while (item->has_dirty()) {
            const auto &[col, rows] = *item->dirty_rows_.begin();
            cell *next = item->find({col, *rows.begin()});
            if (!next) {
                item->set_dirty_mark({col, *rows.begin()}, false);
                continue;
            }
            calculate(*next);
        }
    }
}

rewrite_record workbook::rewrite_formulas(expr::ref_mapper &mapper,
                                          const sheet *skip) {
    rewrite_record record;
    for (const auto &item : sheets_) {
        sheet *owner = item.get();
        if (owner == skip)
            continue;
        owner->for_each_formula([&](cell &formula) {
            const expr::node_ptr moved = formula.formula->rewrite(
                formula.formula, mapper, owner, formula.pos());
            if (moved == formula.formula)
                return;
            record.formulas.push_back({owner, formula.pos(), formula.formula});
            dependents_.remove(formula);
            formula.formula = moved;
        });
    }
    for (const auto &name : names_) {
        if (!name->definition || name->sheet == skip)
            continue;
        expr::node_ptr moved = name->definition->rewrite(
            name->definition, mapper, name->sheet, name->origin);
        if (moved != name->definition) {
            record.names.push_back({name, name->definition});
            name->definition = std::move(moved);
        }
    }
    return record;
}

void workbook::restore_formulas(const rewrite_record &record) {
    for (const formula_change &change : record.formulas)
        if (cell *item = change.where->find(change.pos))
            item->formula = change.old_formula;
    for (const name_change &change : record.names)
        change.name->definition = change.old_definition;
}

void workbook::rebuild_dependencies() {
    dependents_.clear();
    volatile_.clear();
    for (const auto &item : sheets_)
        item->for_each_stored([&](cell &stored) {
            if (stored.formula) {
                attach(stored);
            } else if (stored.in_array) {
                // An element reads its corner, so it goes stale with it.
                stored.reads = {{item.get(),
                                 cell_range::single(stored.array.corner)}};
                dependents_.add(stored);
            }
        });
}

} // namespace book
