#include "core/TeamRotation.h"

#include "utils/Logger.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <format>

namespace overlay::core {

namespace {

bool CaseInsensitiveEqual(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

} // namespace

std::string InferKeyIcon(std::string_view key) {
    if (key.empty()) return "keyboard";
    if (CaseInsensitiveEqual(key, "LButton")) return "mouse_left";
    if (CaseInsensitiveEqual(key, "RButton")) return "mouse_right";
    if (CaseInsensitiveEqual(key, "MButton")) return "mouse_middle";
    if (CaseInsensitiveEqual(key, "XButton1")) return "mouse_x1";
    if (CaseInsensitiveEqual(key, "XButton2")) return "mouse_x2";
    return "keyboard";
}

const CharacterInfo* FindCharacter(const TeamRotation& rotation, std::string_view name) {
    for (const auto& c : rotation.characters) {
        if (c.name == name) return &c;
    }
    return nullptr;
}

void NormalizeTeamRotation(TeamRotation& rotation) {
    // 1. 规范化每个步骤
    for (size_t i = 0; i < rotation.steps.size(); ++i) {
        auto& step = rotation.steps[i];
        if (step.duration_ms < 100) {
            LOG_WARN(std::format("Step {} duration_ms {} clamped to 100", i, step.duration_ms));
            step.duration_ms = 100;
        }
        if (step.key_icon.empty()) {
            step.key_icon = InferKeyIcon(step.key);
        }
        if (!FindCharacter(rotation, step.character)) {
            LOG_WARN(std::format("Step {} references unknown character '{}'", i, step.character));
        }
    }

    // 2. 规范化阶段标记
    if (rotation.stages.empty()) {
        rotation.stages.push_back({"准备", "", 0});
    }
    std::sort(rotation.stages.begin(), rotation.stages.end(),
              [](const StageMarker& a, const StageMarker& b) {
                  return a.start_step < b.start_step;
              });
    if (rotation.stages[0].start_step != 0) {
        LOG_WARN("First stage did not start at 0; inserting default stage.");
        rotation.stages.insert(rotation.stages.begin(), {"准备", "", 0});
    }
    // 去除重复起点的阶段（保留第一个）
    auto last = std::unique(rotation.stages.begin(), rotation.stages.end(),
                            [](const StageMarker& a, const StageMarker& b) {
                                return a.start_step == b.start_step;
                            });
    rotation.stages.erase(last, rotation.stages.end());
}

} // namespace overlay::core
