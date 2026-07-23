#pragma once

#include "core/ConfigManager.h"
#include "core/TeamRotation.h"

#include "imgui.h"

#include <functional>
#include <string>
#include <vector>

namespace overlay::editor {

// 编辑器子组件：队伍流程管理、角色段编排、角色视觉信息编辑
class EditorComponents {
public:
    EditorComponents() = default;

    // 绘制完整编辑器 UI；由 EditorWindow::RenderFrame 每帧调用
    void Draw(overlay::core::ConfigManager* config_manager,
              const std::function<void()>& apply_callback);

private:
    struct Segment {
        size_t start = 0;
        size_t count = 0;
        std::string character;
    };

    void DrawRotations(overlay::core::AppConfig& cfg,
                       overlay::core::ConfigManager* config_manager,
                       const std::function<void()>& apply_callback);
    void DrawSegments(overlay::core::TeamRotation& rotation);
    void DrawCharacters(overlay::core::TeamRotation& rotation);

    static std::vector<Segment> BuildSegments(
        const overlay::core::TeamRotation& rotation);
    static void RebuildStepsFromSegments(
        std::vector<overlay::core::KeyStep>& steps,
        const std::vector<Segment>& segments);

    static ImVec4 HexToColor(const std::string& hex);
    static std::string ColorToHex(const ImVec4& color);
    static bool IsCharacterUsed(const overlay::core::TeamRotation& rotation,
                                std::string_view name);
    static std::string UniqueRotationName(
        const overlay::core::AppConfig& cfg,
        const std::string& base);

    // 面板状态
    int selected_rotation_ = 0;
    int selected_segment_ = -1;
    int selected_character_ = -1;

    bool renaming_ = false;
    int rename_index_ = -1;
    char rename_buffer_[256]{};

    char new_rotation_name_[256] = "\xe6\x96\xb0\xe6\xb5\x81\xe7\xa8\x8b 1"; // "新流程 1"
    char new_character_name_[256] = "\xe6\x96\xb0\xe8\xa7\x92\xe8\x89\xb2"; // "新角色"
    char new_segment_character_[256]{};
    bool show_new_segment_popup_ = false;
};

} // namespace overlay::editor
