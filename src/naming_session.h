#pragma once
#include "model.h"
#include <deque>
#include <optional>
#include <memory>
#include <tuple>

namespace jump {
struct NamingSnapshot {
    NameParts parts;
    std::wstring expression, variables, pattern, replacement;
    int counter = 0, digits = 3, dateFormat = 0, ruleIndex = 1;
    bool folder = false, variablesEnabled = false, upper = false, ruleEnabled = false, regex = false;
    StructuredNaming structure;
    std::shared_ptr<NamingSnapshot> recovery;
    bool operator==(const NamingSnapshot &b) const {
        return (!recovery && !b.recovery || (recovery && b.recovery && *recovery == *b.recovery)) &&
               structure == b.structure &&
               std::tie(parts.base, parts.stage, parts.date, parts.prefix, parts.suffix, expression,
                        variables, pattern, replacement, counter, digits, dateFormat, ruleIndex, folder,
                        variablesEnabled, upper, ruleEnabled,
                        regex) == std::tie(b.parts.base, b.parts.stage, b.parts.date, b.parts.prefix,
                                           b.parts.suffix, b.expression, b.variables, b.pattern,
                                           b.replacement, b.counter, b.digits, b.dateFormat, b.ruleIndex,
                                           b.folder, b.variablesEnabled, b.upper, b.ruleEnabled, b.regex);
    }
};
class NamingHistory {
    struct Entry {
        NamingSnapshot state;
        unsigned long long serial;
    };
    std::deque<Entry> undoStates, redoStates;
    unsigned long long serial = 0;

  public:
    void clear() {
        undoStates.clear();
        redoStates.clear();
    }
    bool canUndo() const {
        return !undoStates.empty();
    }
    bool canRedo() const {
        return !redoStates.empty();
    }
    unsigned long long mark() const {
        return serial + 1;
    }
    void cancelField(unsigned long long since, const std::wstring &schema, const NamingField &original,
                     const StructuredNaming *baseline = nullptr) {
        for (auto *states : {&undoStates, &redoStates})
            for (auto &entry : *states) {
                if (entry.serial < since || entry.state.structure.id != schema)
                    continue;
                if (baseline)
                    entry.state.structure = *baseline;
                else
                    for (auto &field : entry.state.structure.fields)
                        if (field.id == original.id)
                            field = original;
            }
    }
    void checkpoint(const NamingSnapshot &state) {
        if (undoStates.empty() || !(undoStates.back().state == state))
            undoStates.push_back({state, ++serial});
        if (undoStates.size() > 80)
            undoStates.pop_front();
        redoStates.clear();
    }
    std::optional<NamingSnapshot> move(const NamingSnapshot &current, bool redo) {
        auto &from = redo ? redoStates : undoStates;
        auto &to = redo ? undoStates : redoStates;
        while (!from.empty() && from.back().state == current)
            from.pop_back();
        if (from.empty())
            return {};
        auto next = from.back().state;
        from.pop_back();
        to.push_back({current, ++serial});
        return next;
    }
};
} // namespace jump
