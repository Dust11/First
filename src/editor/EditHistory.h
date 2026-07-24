#pragma once

#include "core/ConfigManager.h"

#include <vector>

namespace overlay::editor {

class EditHistory {
public:
    EditHistory() = default;

    // Push a post-edit snapshot. Truncates any redo branch.
    void Push(const overlay::core::AppConfig& snapshot) {
        if (index_ + 1 < static_cast<int>(stack_.size())) {
            stack_.resize(static_cast<size_t>(index_) + 1);
        }
        stack_.push_back(snapshot);
        ++index_;
        if (stack_.size() > kMaxSize) {
            stack_.erase(stack_.begin());
            --index_;
        }
    }

    bool CanUndo() const { return index_ > 0; }
    bool CanRedo() const {
        return index_ + 1 < static_cast<int>(stack_.size());
    }

    // Return the snapshot to restore. Caller assigns to ConfigManager and calls apply.
    overlay::core::AppConfig Undo() {
        if (CanUndo()) {
            --index_;
            return stack_[static_cast<size_t>(index_)];
        }
        return {};
    }

    overlay::core::AppConfig Redo() {
        if (CanRedo()) {
            ++index_;
            return stack_[static_cast<size_t>(index_)];
        }
        return {};
    }

    void Clear() {
        stack_.clear();
        index_ = -1;
    }

private:
    std::vector<overlay::core::AppConfig> stack_;
    int index_ = -1;                     // -1 = empty
    static constexpr size_t kMaxSize = 50;
};

} // namespace overlay::editor
