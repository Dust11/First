#include "editor/EditorComponents.h"

#include "utils/Logger.h"
#include "utils/ResourceLoader.h"
#include "utils/TextEncoding.h"

#include <algorithm>
#include <charconv>
#include <commdlg.h>
#include <cstdio>
#include <cstring>
#include <d3d11.h>
#include <filesystem>
#include <format>
#include <utility>
#include <wrl/client.h>

namespace overlay::editor {

namespace fs = std::filesystem;

namespace {

constexpr const char* kSegmentPayload = "SEGMENT_DND";
constexpr const char* kStepPayload = "STEP_DND";

struct IconOption {
    const char* label;
    const char* value;
};

constexpr IconOption kIconOptions[] = {
    {"\xe8\x87\xaa\xe5\x8a\xa8\xe6\x8e\xa8\xe6\x96\xad", ""},
    {"\xe9\xbc\xa0\xe6\xa0\x87\xe5\xb7\xa6\xe9\x94\xae", "mouse_left"},
    {"\xe9\xbc\xa0\xe6\xa0\x87\xe5\xb7\xa6\xe9\x94\xae\xe9\x95\xbf\xe6\x8c\x89", "mouse_left_hold"},
    {"\xe9\xbc\xa0\xe6\xa0\x87\xe5\x8f\xb3\xe9\x94\xae", "mouse_right"},
    {"\xe9\xbc\xa0\xe6\xa0\x87\xe4\xb8\xad\xe9\x94\xae", "mouse_middle"},
    {"\xe9\xbc\xa0\xe6\xa0\x87\xe4\xbe\xa7\xe9\x94\xae 1", "mouse_x1"},
    {"\xe9\xbc\xa0\xe6\xa0\x87\xe4\xbe\xa7\xe9\x94\xae 2", "mouse_x2"},
    {"\xe9\x94\xae\xe7\x9b\x98", "keyboard"},
    {"\xe8\x87\xaa\xe5\xae\x9a\xe4\xb9\x89\xe5\x9b\xbe\xe6\xa0\x87", "custom"},
};

} // namespace

void EditorComponents::ClearTransientState() {
    stage_to_delete_ = -1;
    step_to_delete_ = -1;
    step_to_duplicate_ = -1;
    step_move_src_ = -1;
    step_move_dst_ = -1;
    renaming_ = false;
    rename_buffer_[0] = '\0';
    bg_path_buffer_[0] = '\0';
}

void EditorComponents::OnEditorHidden() {
    history_.Clear();
    history_seeded_ = false;
    ClearTransientState();
}

void EditorComponents::Draw(overlay::core::ConfigManager* config_manager,
                            ID3D11Device* device,
                            const std::function<void()>& apply_callback) {
    if (!config_manager) return;
    device_ = device;

    if (!resource_loader_initialized_) {
        resource_loader_.Initialize();
        resource_loader_initialized_ = true;
    }

    auto& cfg = config_manager->GetConfig();
    if (cfg.rotations.empty()) {
        // 至少保留一个默认流程
        overlay::core::TeamRotation rot;
        rot.name = "\xe9\xbb\x98\xe8\xae\xa4\xe6\xb5\x81\xe7\xa8\x8b"; // "默认流程"
        rot.characters = {{"\xe6\x96\xb0\xe8\xa7\x92\xe8\x89\xb2", "", "#888888"}};
        rot.stages = {{"\xe5\x87\x86\xe5\xa4\x87", "", 0}};
        rot.steps = {{"\xe6\x96\xb0\xe8\xa7\x92\xe8\x89\xb2", "LButton", "\xe6\x96\xb0\xe6\xad\xa5\xe9\xaa\xa4", "", 1000}};
        cfg.rotations.push_back(std::move(rot));
        cfg.active_rotation = cfg.rotations[0].name;
    }

    if (selected_rotation_ >= static_cast<int>(cfg.rotations.size())) {
        selected_rotation_ = static_cast<int>(cfg.rotations.size()) - 1;
    }
    if (selected_rotation_ < 0) selected_rotation_ = 0;

    if (!history_seeded_) {
        history_.Push(cfg);
        history_seeded_ = true;
    }

    auto& rotation = cfg.rotations[selected_rotation_];

    ImGui::Begin("\xe9\x98\x9f\xe4\xbc\x8d\xe6\xb5\x81\xe7\xa8\x8b\xe7\xbc\x96\xe8\xbe\x91\xe5\x99\xa8"); // 队伍流程编辑器

    // Undo / Redo 快捷键（需处于窗口上下文中才能检测焦点）
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
        !ImGui::GetIO().WantTextInput) {
        if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Z) && history_.CanUndo()) {
            cfg = history_.Undo();
            if (apply_callback) apply_callback();
            ClearTransientState();
            if (selected_rotation_ >= static_cast<int>(cfg.rotations.size())) {
                selected_rotation_ = static_cast<int>(cfg.rotations.size()) - 1;
            }
            if (selected_rotation_ < 0) selected_rotation_ = 0;
            rotation = cfg.rotations[selected_rotation_];
        } else if ((ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Y) ||
                    ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Z)) &&
                   history_.CanRedo()) {
            cfg = history_.Redo();
            if (apply_callback) apply_callback();
            ClearTransientState();
            if (selected_rotation_ >= static_cast<int>(cfg.rotations.size())) {
                selected_rotation_ = static_cast<int>(cfg.rotations.size()) - 1;
            }
            if (selected_rotation_ < 0) selected_rotation_ = 0;
            rotation = cfg.rotations[selected_rotation_];
        }
    }

    if (ImGui::BeginTable("editor_layout", 3,
                          ImGuiTableFlags_Resizable |
                              ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupColumn("\xe6\xb5\x81\xe7\xa8\x8b", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("\xe8\xa7\x92\xe8\x89\xb2\xe6\xae\xb5", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("\xe8\xa7\x92\xe8\x89\xb2\xe8\xa7\x86\xe8\xa7\x89", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        ImGui::TableNextColumn();
        DrawRotations(cfg, config_manager, apply_callback);

        ImGui::TableNextColumn();
        DrawSegments(cfg, rotation);

        ImGui::TableNextColumn();
        DrawCharacters(cfg, rotation);

        ImGui::EndTable();
    }

    // 下方：阶段标记 + 按键序列
    ImVec2 bottom_size = ImVec2(0.0f, ImGui::GetContentRegionAvail().y);
    if (bottom_size.y > 0.0f &&
        ImGui::BeginTable("editor_bottom", 2,
                          ImGuiTableFlags_Resizable |
                              ImGuiTableFlags_BordersInnerV |
                              ImGuiTableFlags_SizingStretchProp,
                          bottom_size)) {
        ImGui::TableSetupColumn("\xe9\x98\xb6\xe6\xae\xb5\xe6\xa0\x87\xe8\xae\xb0",
                                ImGuiTableColumnFlags_WidthStretch, 0.35f);
        ImGui::TableSetupColumn("\xe6\x8c\x89\xe9\x94\xae\xe5\xba\x8f\xe5\x88\x97",
                                ImGuiTableColumnFlags_WidthStretch, 0.65f);
        ImGui::TableHeadersRow();

        ImGui::TableNextColumn();
        DrawStages(cfg, rotation);

        ImGui::TableNextColumn();
        DrawKeySequence(cfg, rotation);

        ImGui::EndTable();
    }
    ImGui::End();

    // 背景图设置窗口（可停靠）
    if (ImGui::Begin("\xe8\x83\x8c\xe6\x99\xaf\xe5\x9b\xbe\xe8\xae\xbe\xe7\xbd\xae")) {
        DrawBackgroundImage(config_manager, rotation);
    }
    ImGui::End();

    // 设置窗口（可停靠）
    if (ImGui::Begin("\xe8\xae\xbe\xe7\xbd\xae")) {
        DrawSettings(cfg);
    }
    ImGui::End();
}

void EditorComponents::DrawRotations(
    overlay::core::AppConfig& cfg,
    overlay::core::ConfigManager* config_manager,
    const std::function<void()>& apply_callback) {
    ImGui::TextUnformatted("\xe6\xb5\x81\xe7\xa8\x8b\xe5\x88\x97\xe8\xa1\xa8"); // 流程列表

    if (ImGui::Button("\xe6\x96\xb0\xe5\xbb\xba")) { // 新建
        overlay::core::TeamRotation rot;
        rot.name = UniqueRotationName(cfg, "\xe6\x96\xb0\xe6\xb5\x81\xe7\xa8\x8b"); // 新流程
        rot.characters = {{"\xe6\x96\xb0\xe8\xa7\x92\xe8\x89\xb2", "", "#888888"}};
        rot.stages = {{"\xe5\x87\x86\xe5\xa4\x87", "", 0}};
        rot.steps = {{"\xe6\x96\xb0\xe8\xa7\x92\xe8\x89\xb2", "LButton", "\xe6\x96\xb0\xe6\xad\xa5\xe9\xaa\xa4", "", 1000}};
        cfg.rotations.push_back(std::move(rot));
        selected_rotation_ = static_cast<int>(cfg.rotations.size()) - 1;
        history_.Push(cfg);
    }
    ImGui::SameLine();
    if (ImGui::Button("\xe8\xae\xbe\xe4\xb8\xba\xe5\xbd\x93\xe5\x89\x8d") && selected_rotation_ >= 0) { // 设为当前
        cfg.active_rotation = cfg.rotations[selected_rotation_].name;
        history_.Push(cfg);
    }
    ImGui::SameLine();
    if (ImGui::Button("\xe4\xbf\x9d\xe5\xad\x98")) { // 保存
        if (config_manager->Save()) {
            LOG_INFO("Config saved from editor.");
            if (apply_callback) apply_callback();
        }
    }

    ImGui::BeginChild("rotation_list", ImVec2(0, 0), true,
                      ImGuiWindowFlags_HorizontalScrollbar);
    for (int i = 0; i < static_cast<int>(cfg.rotations.size()); ++i) {
        ImGui::PushID(i);
        const bool is_active = cfg.rotations[i].name == cfg.active_rotation;
        std::string label = cfg.rotations[i].name;
        if (is_active) label += " [\xe5\xbd\x93\xe5\x89\x8d]";

        if (ImGui::Selectable(label.c_str(), selected_rotation_ == i)) {
            selected_rotation_ = i;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("\xe9\x87\x8d\xe5\x91\xbd\xe5\x90\x8d")) { // 重命名
            rename_index_ = i;
            std::snprintf(rename_buffer_, sizeof(rename_buffer_), "%s",
                          cfg.rotations[i].name.c_str());
            ImGui::OpenPopup("rename_rotation");
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("\xe5\x88\xa0\xe9\x99\xa4")) { // 删除
            if (cfg.rotations.size() > 1) {
                bool was_active = cfg.rotations[i].name == cfg.active_rotation;
                cfg.rotations.erase(cfg.rotations.begin() + i);
                if (selected_rotation_ >= static_cast<int>(cfg.rotations.size())) {
                    selected_rotation_ = static_cast<int>(cfg.rotations.size()) - 1;
                }
                if (was_active && !cfg.rotations.empty()) {
                    cfg.active_rotation = cfg.rotations[0].name;
                }
                history_.Push(cfg);
            }
        }

        if (renaming_ && rename_index_ == i) {
            ImGui::OpenPopup("rename_rotation");
            renaming_ = false;
        }
        if (ImGui::BeginPopup("rename_rotation")) {
            ImGui::InputText("\xe6\x96\xb0\xe5\x90\x8d\xe7\xa7\xb0", rename_buffer_,
                             sizeof(rename_buffer_));
            if (ImGui::Button("\xe7\xa1\xae\xe5\xae\x9a")) { // 确定
                std::string old_name = cfg.rotations[i].name;
                std::string new_name = rename_buffer_;
                if (!new_name.empty()) {
                    cfg.rotations[i].name = new_name;
                    if (cfg.active_rotation == old_name) {
                        cfg.active_rotation = new_name;
                    }
                    history_.Push(cfg);
                }
                rename_index_ = -1;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("\xe5\x8f\x96\xe6\xb6\x88")) { // 取消
                rename_index_ = -1;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        ImGui::PopID();
    }
    ImGui::EndChild();
}

void EditorComponents::DrawSegments(overlay::core::AppConfig& cfg,
                                    overlay::core::TeamRotation& rotation) {
    auto segments = BuildSegments(rotation);
    if (selected_segment_ >= static_cast<int>(segments.size())) {
        selected_segment_ = -1;
    }

    ImGui::Text("\xe8\xa7\x92\xe8\x89\xb2\xe6\xae\xb5 (%d \xe6\xae\xb5, %zu \xe6\xad\xa5)", // 角色段
                static_cast<int>(segments.size()), rotation.steps.size());

    if (ImGui::Button("\xe6\xb7\xbb\xe5\x8a\xa0\xe6\xae\xb5")) { // 添加段
        show_new_segment_popup_ = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("\xe5\x88\xa0\xe9\x99\xa4\xe6\xae\xb5") && // 删除段
        selected_segment_ >= 0) {
        auto segs = BuildSegments(rotation);
        auto it = segs.begin() + selected_segment_;
        rotation.steps.erase(rotation.steps.begin() + it->start,
                             rotation.steps.begin() + it->start + it->count);
        selected_segment_ = -1;
        history_.Push(cfg);
    }

    if (show_new_segment_popup_) {
        ImGui::OpenPopup("add_segment_popup");
        show_new_segment_popup_ = false;
        new_segment_character_[0] = '\0';
    }

    if (ImGui::BeginPopupModal("add_segment_popup", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("\xe9\x80\x89\xe6\x8b\xa9\xe8\xa7\x92\xe8\x89\xb2"); // 选择角色
        for (const auto& c : rotation.characters) {
            if (ImGui::Button(c.name.c_str())) {
                overlay::core::KeyStep step;
                step.character = c.name;
                step.key = "LButton";
                step.skill_name = "\xe6\x96\xb0\xe6\xad\xa5\xe9\xaa\xa4"; // 新步骤
                step.duration_ms = 1000;
                rotation.steps.push_back(std::move(step));
                selected_segment_ = static_cast<int>(BuildSegments(rotation).size()) - 1;
                ImGui::CloseCurrentPopup();
                history_.Push(cfg);
            }
        }
        ImGui::Separator();
        ImGui::InputText("\xe6\x96\xb0\xe8\xa7\x92\xe8\x89\xb2\xe5\x90\x8d", // 新角色名
                         new_segment_character_, sizeof(new_segment_character_));
        if (ImGui::Button("\xe4\xbd\xbf\xe7\x94\xa8\xe6\x96\xb0\xe8\xa7\x92\xe8\x89\xb2")) { // 使用新角色
            std::string name = new_segment_character_;
            if (!name.empty()) {
                overlay::core::KeyStep step;
                step.character = name;
                step.key = "LButton";
                step.skill_name = "\xe6\x96\xb0\xe6\xad\xa5\xe9\xaa\xa4";
                step.duration_ms = 1000;
                rotation.steps.push_back(std::move(step));
                // 自动添加角色视觉信息（如果不存在）
                if (!overlay::core::FindCharacter(rotation, name)) {
                    rotation.characters.push_back({name, "", "#888888"});
                }
                selected_segment_ = static_cast<int>(BuildSegments(rotation).size()) - 1;
                history_.Push(cfg);
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("\xe5\x8f\x96\xe6\xb6\x88")) { // 取消
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::BeginChild("segment_list", ImVec2(0, 0), true);
    for (int i = 0; i < static_cast<int>(segments.size()); ++i) {
        ImGui::PushID(i);
        const auto& seg = segments[i];
        const auto* char_info = overlay::core::FindCharacter(rotation, seg.character);
        ImVec4 color = char_info ? HexToColor(char_info->theme_color)
                                 : ImVec4(0.5f, 0.5f, 0.5f, 1.0f);

        ImGui::ColorButton("##color", color,
                           ImGuiColorEditFlags_NoTooltip |
                               ImGuiColorEditFlags_NoDragDrop,
                           ImVec2(16, 16));
        ImGui::SameLine();

        char label[256];
        std::snprintf(label, sizeof(label), "[%s \xc3\x97%zu]", // ×
                      seg.character.c_str(), seg.count);
        if (ImGui::Selectable(label, selected_segment_ == i)) {
            selected_segment_ = i;
        }

        if (ImGui::BeginDragDropSource()) {
            int src_index = i;
            ImGui::SetDragDropPayload(kSegmentPayload, &src_index,
                                      sizeof(int));
            ImGui::TextUnformatted(label);
            ImGui::EndDragDropSource();
        }
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload =
                    ImGui::AcceptDragDropPayload(kSegmentPayload)) {
                IM_ASSERT(payload->DataSize == sizeof(int));
                int src = *static_cast<const int*>(payload->Data);
                int dst = i;
                if (src != dst) {
                    auto segs = BuildSegments(rotation);
                    Segment moved = std::move(segs[src]);
                    if (src < dst) {
                        std::move(segs.begin() + src + 1,
                                  segs.begin() + dst + 1,
                                  segs.begin() + src);
                    } else {
                        std::move(segs.begin() + dst, segs.begin() + src,
                                  segs.begin() + dst + 1);
                    }
                    segs[dst] = std::move(moved);
                    RebuildStepsFromSegments(rotation.steps, segs);
                    selected_segment_ = dst;
                    history_.Push(cfg);
                }
            }
            ImGui::EndDragDropTarget();
        }
        ImGui::PopID();
    }
    ImGui::EndChild();
}

void EditorComponents::DrawCharacters(overlay::core::AppConfig& cfg,
                                      overlay::core::TeamRotation& rotation) {
    ImGui::TextUnformatted("\xe8\xa7\x92\xe8\x89\xb2\xe8\xa7\x86\xe8\xa7\x89\xe4\xbf\xa1\xe6\x81\xaf"); // 角色视觉信息

    if (ImGui::Button("\xe6\xb7\xbb\xe5\x8a\xa0\xe8\xa7\x92\xe8\x89\xb2")) { // 添加角色
        std::string name = new_character_name_;
        if (!name.empty() && !overlay::core::FindCharacter(rotation, name)) {
            rotation.characters.push_back({name, "", "#888888"});
            selected_character_ = static_cast<int>(rotation.characters.size()) - 1;
            history_.Push(cfg);
        }
    }
    ImGui::SameLine();
    ImGui::InputText("##new_char", new_character_name_,
                     sizeof(new_character_name_));

    ImGui::BeginChild("character_list", ImVec2(0, 150), true);
    for (int i = 0; i < static_cast<int>(rotation.characters.size()); ++i) {
        ImGui::PushID(i);
        const auto& c = rotation.characters[i];
        ImVec4 color = HexToColor(c.theme_color);
        ImGui::ColorButton("##preview", color,
                           ImGuiColorEditFlags_NoTooltip |
                               ImGuiColorEditFlags_NoDragDrop,
                           ImVec2(14, 14));
        ImGui::SameLine();
        if (ImGui::Selectable(c.name.c_str(), selected_character_ == i)) {
            selected_character_ = i;
        }
        ImGui::PopID();
    }
    ImGui::EndChild();

    if (selected_character_ >= 0 &&
        selected_character_ < static_cast<int>(rotation.characters.size())) {
        auto& c = rotation.characters[selected_character_];

        std::string old_name = c.name;
        std::string name = c.name;
        name.resize(256);
        if (ImGui::InputText("\xe8\xa7\x92\xe8\x89\xb2\xe5\x90\x8d", name.data(), name.size())) { // 角色名
            c.name = name.c_str();
            if (c.name != old_name) {
                // 同步更新所有步骤中的角色引用
                for (auto& step : rotation.steps) {
                    if (step.character == old_name) step.character = c.name;
                }
            }
        }
        if (ImGui::IsItemDeactivatedAfterEdit()) history_.Push(cfg);

        std::string avatar = c.avatar_image;
        avatar.resize(512);
        if (ImGui::InputText("\xe5\xa4\xb4\xe5\x83\x8f\xe8\xb7\xaf\xe5\xbe\x84", avatar.data(), // 头像路径
                             avatar.size())) {
            c.avatar_image = avatar.c_str();
        }
        if (ImGui::IsItemDeactivatedAfterEdit()) history_.Push(cfg);

        ImVec4 color = HexToColor(c.theme_color);
        if (ImGui::ColorEdit3("\xe4\xb8\xbb\xe9\xa2\x98\xe8\x89\xb2", &color.x)) { // 主题色
            c.theme_color = ColorToHex(color);
        }
        if (ImGui::IsItemDeactivatedAfterEdit()) history_.Push(cfg);
    }

    if (selected_character_ >= 0 &&
        selected_character_ < static_cast<int>(rotation.characters.size())) {
        const auto& c = rotation.characters[selected_character_];
        if (ImGui::Button("\xe5\x88\xa0\xe9\x99\xa4\xe8\xa7\x92\xe8\x89\xb2")) { // 删除角色
            if (!IsCharacterUsed(rotation, c.name)) {
                rotation.characters.erase(rotation.characters.begin() +
                                          selected_character_);
                if (selected_character_ >=
                    static_cast<int>(rotation.characters.size())) {
                    selected_character_ =
                        static_cast<int>(rotation.characters.size()) - 1;
                }
                history_.Push(cfg);
            } else {
                LOG_WARN(std::format("Cannot delete character '{}': still in use.",
                                     c.name));
            }
        }
    }
}

void EditorComponents::DrawStages(overlay::core::AppConfig& cfg,
                                  overlay::core::TeamRotation& rotation) {
    ImGui::TextUnformatted("\xe9\x98\xb6\xe6\xae\xb5\xe6\xa0\x87\xe8\xae\xb0"); // 阶段标记

    if (ImGui::Button("\xe6\xb7\xbb\xe5\x8a\xa0\xe9\x98\xb6\xe6\xae\xb5")) { // 添加阶段
        size_t start = (selected_step_ >= 0)
                           ? static_cast<size_t>(selected_step_)
                           : rotation.steps.size();
        if (start > rotation.steps.size()) start = rotation.steps.size();
        rotation.stages.push_back({"\xe6\x96\xb0\xe9\x98\xb6\xe6\xae\xb5", "", start}); // 新阶段
        std::sort(rotation.stages.begin(), rotation.stages.end(),
                  [](const overlay::core::StageMarker& a,
                     const overlay::core::StageMarker& b) {
                      return a.start_step < b.start_step;
                  });
        if (!rotation.stages.empty() && rotation.stages[0].start_step != 0) {
            rotation.stages[0].start_step = 0;
        }
        selected_stage_ = -1;
        history_.Push(cfg);
    }

    ImGui::BeginChild("stage_list", ImVec2(0, ImGui::GetContentRegionAvail().y), true);
    for (int i = 0; i < static_cast<int>(rotation.stages.size()); ++i) {
        ImGui::PushID(i);
        auto& stage = rotation.stages[i];

        // 阶段名称
        std::string label = stage.label;
        label.resize(128);
        if (ImGui::InputText("\xe5\x90\x8d\xe7\xa7\xb0", label.data(), label.size())) { // 名称
            stage.label = label.c_str();
        }
        if (ImGui::IsItemDeactivatedAfterEdit()) history_.Push(cfg);

        // 起始步骤
        int start = static_cast<int>(stage.start_step);
        if (ImGui::InputInt("\xe8\xb5\xb7\xe5\xa7\x8b\xe6\xad\xa5", &start)) { // 起始步
            int max_step = static_cast<int>(rotation.steps.size());
            start = std::clamp(start, 0, max_step);
            stage.start_step = static_cast<size_t>(start);
            std::sort(rotation.stages.begin(), rotation.stages.end(),
                      [](const overlay::core::StageMarker& a,
                         const overlay::core::StageMarker& b) {
                          return a.start_step < b.start_step;
                      });
            if (!rotation.stages.empty() && rotation.stages[0].start_step != 0) {
                rotation.stages[0].start_step = 0;
            }
            selected_stage_ = -1;
        }
        if (ImGui::IsItemDeactivatedAfterEdit()) history_.Push(cfg);

        // 颜色覆盖
        bool auto_color = stage.color.empty();
        if (ImGui::Checkbox("\xe8\x87\xaa\xe5\x8a\xa8\xe9\xa2\x9c\xe8\x89\xb2", &auto_color)) { // 自动颜色
            if (auto_color) {
                stage.color.clear();
            } else {
                stage.color = "#888888";
            }
            history_.Push(cfg);
        }
        if (!auto_color) {
            ImVec4 col = HexToColor(stage.color);
            if (ImGui::ColorEdit3("\xe9\xa2\x9c\xe8\x89\xb2", &col.x)) { // 颜色
                stage.color = ColorToHex(col);
            }
            if (ImGui::IsItemDeactivatedAfterEdit()) history_.Push(cfg);
        }

        ImGui::SameLine();
        if (ImGui::SmallButton("\xe5\x88\xa0\xe9\x99\xa4") && // 删除
            rotation.stages.size() > 1) {
            stage_to_delete_ = i;
        }

        ImGui::Separator();
        ImGui::PopID();
    }
    ImGui::EndChild();

    if (stage_to_delete_ >= 0) {
        if (rotation.stages.size() > 1) {
            rotation.stages.erase(rotation.stages.begin() + stage_to_delete_);
            if (selected_stage_ == stage_to_delete_) selected_stage_ = -1;
            else if (selected_stage_ > stage_to_delete_) --selected_stage_;
            history_.Push(cfg);
        }
        stage_to_delete_ = -1;
    }
}

void EditorComponents::DrawKeySequence(overlay::core::AppConfig& cfg,
                                       overlay::core::TeamRotation& rotation) {
    ImGui::Text("\xe6\x8c\x89\xe9\x94\xae\xe5\xba\x8f\xe5\x88\x97 (%zu \xe6\xad\xa5)", rotation.steps.size()); // 按键序列

    if (ImGui::Button("\xe6\xb7\xbb\xe5\x8a\xa0\xe6\xad\xa5\xe9\xaa\xa4")) { // 添加步骤
        overlay::core::KeyStep step;
        if (selected_character_ >= 0 &&
            selected_character_ < static_cast<int>(rotation.characters.size())) {
            step.character = rotation.characters[selected_character_].name;
        } else if (!rotation.steps.empty()) {
            step.character = rotation.steps.back().character;
        } else if (!rotation.characters.empty()) {
            step.character = rotation.characters[0].name;
        } else {
            step.character = "\xe6\x96\xb0\xe8\xa7\x92\xe8\x89\xb2"; // 新角色
        }
        step.key = "LButton";
        step.skill_name = "\xe6\x96\xb0\xe6\xad\xa5\xe9\xaa\xa4"; // 新步骤
        step.duration_ms = 1000;

        size_t insert_pos = rotation.steps.size();
        if (selected_step_ >= 0 &&
            selected_step_ < static_cast<int>(rotation.steps.size())) {
            insert_pos = static_cast<size_t>(selected_step_) + 1;
        }
        rotation.steps.insert(rotation.steps.begin() + insert_pos, std::move(step));
        selected_step_ = static_cast<int>(insert_pos);
        history_.Push(cfg);
    }
    ImGui::SameLine();
    if (ImGui::Button("\xe5\x88\xa0\xe9\x99\xa4\xe9\x80\x89\xe4\xb8\xad") && // 删除选中
        selected_step_ >= 0) {
        step_to_delete_ = selected_step_;
    }
    ImGui::SameLine();
    if (ImGui::Button("\xe5\xa4\x8d\xe5\x88\xb6\xe9\x80\x89\xe4\xb8\xad") && // 复制选中
        selected_step_ >= 0) {
        step_to_duplicate_ = selected_step_;
    }

    float table_height = ImGui::GetContentRegionAvail().y;
    if (table_height < 100.0f) table_height = 100.0f;

    ImGuiTableFlags table_flags = ImGuiTableFlags_Borders |
                                  ImGuiTableFlags_Resizable |
                                  ImGuiTableFlags_SizingStretchProp |
                                  ImGuiTableFlags_ScrollY |
                                  ImGuiTableFlags_RowBg;
    if (ImGui::BeginTable("step_list", 8, table_flags,
                          ImVec2(0.0f, table_height))) {
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 30.0f);
        ImGui::TableSetupColumn("\xe8\xa7\x92\xe8\x89\xb2", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("\xe6\x8c\x89\xe9\x94\xae", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("\xe6\x8a\x80\xe8\x83\xbd", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("\xe5\x9b\xbe\xe6\xa0\x87", ImGuiTableColumnFlags_WidthFixed, 110.0f);
        ImGui::TableSetupColumn("\xe8\x87\xaa\xe5\xae\x9a\xe4\xb9\x89\xe5\x9b\xbe\xe6\xa0\x87",
                                ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("\xe6\x8c\x81\xe7\xbb\xad(ms)", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("\xe6\x93\x8d\xe4\xbd\x9c", ImGuiTableColumnFlags_WidthFixed, 70.0f);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();

        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(rotation.steps.size()),
                      ImGui::GetTextLineHeightWithSpacing());
        while (clipper.Step()) {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                ImGui::TableNextRow();
                ImGui::PushID(i);
                auto& step = rotation.steps[i];

                // # / 选择
                ImGui::TableSetColumnIndex(0);
                char idx_label[16];
                std::snprintf(idx_label, sizeof(idx_label), "%03d", i + 1);
                if (ImGui::Selectable(idx_label, selected_step_ == i,
                                      ImGuiSelectableFlags_AllowOverlap)) {
                    selected_step_ = i;
                }
                if (ImGui::BeginDragDropSource()) {
                    int payload_idx = i;
                    ImGui::SetDragDropPayload(kStepPayload, &payload_idx, sizeof(int));
                    ImGui::Text("%s %s", step.character.c_str(), step.skill_name.c_str());
                    ImGui::EndDragDropSource();
                }
                if (ImGui::BeginDragDropTarget()) {
                    if (const ImGuiPayload* payload =
                            ImGui::AcceptDragDropPayload(kStepPayload)) {
                        IM_ASSERT(payload->DataSize == sizeof(int));
                        int src = *static_cast<const int*>(payload->Data);
                        step_move_src_ = src;
                        step_move_dst_ = i;
                    }
                    ImGui::EndDragDropTarget();
                }

                // 角色
                ImGui::TableSetColumnIndex(1);
                const char* preview = step.character.c_str();
                if (ImGui::BeginCombo("##char", preview)) {
                    for (const auto& c : rotation.characters) {
                        bool selected = (step.character == c.name);
                        if (ImGui::Selectable(c.name.c_str(), selected)) {
                            step.character = c.name;
                            history_.Push(cfg);
                        }
                        if (selected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }

                // 按键
                ImGui::TableSetColumnIndex(2);
                std::string key = step.key;
                key.resize(64);
                if (ImGui::InputText("##key", key.data(), key.size())) {
                    step.key = key.c_str();
                }
                if (ImGui::IsItemDeactivatedAfterEdit()) history_.Push(cfg);

                // 技能名
                ImGui::TableSetColumnIndex(3);
                std::string skill = step.skill_name;
                skill.resize(128);
                if (ImGui::InputText("##skill", skill.data(), skill.size())) {
                    step.skill_name = skill.c_str();
                }
                if (ImGui::IsItemDeactivatedAfterEdit()) history_.Push(cfg);

                // 图标类型
                ImGui::TableSetColumnIndex(4);
                int icon_index = 0;
                for (int k = 0; k < IM_ARRAYSIZE(kIconOptions); ++k) {
                    if (step.key_icon == kIconOptions[k].value) {
                        icon_index = k;
                        break;
                    }
                }
                if (ImGui::Combo("##icon", &icon_index, [](void* data, int idx) {
                        auto* opts = static_cast<const IconOption*>(data);
                        return opts[idx].label;
                    },
                    const_cast<void*>(static_cast<const void*>(kIconOptions)),
                    IM_ARRAYSIZE(kIconOptions))) {
                    step.key_icon = kIconOptions[icon_index].value;
                    history_.Push(cfg);
                }

                // 自定义图标路径
                ImGui::TableSetColumnIndex(5);
                bool is_custom = (step.key_icon == "custom");
                ImGui::BeginDisabled(!is_custom);
                std::string custom = step.custom_icon;
                custom.resize(256);
                if (ImGui::InputText("##custom", custom.data(), custom.size())) {
                    step.custom_icon = custom.c_str();
                }
                if (ImGui::IsItemDeactivatedAfterEdit()) history_.Push(cfg);
                ImGui::EndDisabled();

                // 持续时间
                ImGui::TableSetColumnIndex(6);
                int duration = step.duration_ms;
                if (ImGui::InputInt("##dur", &duration)) {
                    step.duration_ms = std::max(duration, 100);
                }
                if (ImGui::IsItemDeactivatedAfterEdit()) history_.Push(cfg);

                // 操作
                ImGui::TableSetColumnIndex(7);
                if (ImGui::SmallButton("\xe5\x88\xa0")) { // 删
                    step_to_delete_ = i;
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("\xe5\xa4\x8d")) { // 复
                    step_to_duplicate_ = i;
                }

                ImGui::PopID();
            }
        }
        ImGui::EndTable();
    }

    // 处理延迟操作
    if (step_to_delete_ >= 0 &&
        step_to_delete_ < static_cast<int>(rotation.steps.size())) {
        rotation.steps.erase(rotation.steps.begin() + step_to_delete_);
        if (selected_step_ == step_to_delete_) selected_step_ = -1;
        else if (selected_step_ > step_to_delete_) --selected_step_;
        step_to_delete_ = -1;
        history_.Push(cfg);
    }

    if (step_to_duplicate_ >= 0 &&
        step_to_duplicate_ < static_cast<int>(rotation.steps.size())) {
        const auto& src = rotation.steps[step_to_duplicate_];
        rotation.steps.insert(rotation.steps.begin() + step_to_duplicate_ + 1, src);
        if (selected_step_ == step_to_duplicate_) selected_step_ = step_to_duplicate_ + 1;
        else if (selected_step_ > step_to_duplicate_) ++selected_step_;
        step_to_duplicate_ = -1;
        history_.Push(cfg);
    }

    if (step_move_src_ >= 0 && step_move_dst_ >= 0 &&
        step_move_src_ < static_cast<int>(rotation.steps.size()) &&
        step_move_dst_ < static_cast<int>(rotation.steps.size()) &&
        step_move_src_ != step_move_dst_) {
        size_t src = static_cast<size_t>(step_move_src_);
        size_t dst = static_cast<size_t>(step_move_dst_);
        auto item = std::move(rotation.steps[src]);
        if (src < dst) {
            std::move(rotation.steps.begin() + src + 1,
                      rotation.steps.begin() + dst + 1,
                      rotation.steps.begin() + src);
        } else {
            std::move(rotation.steps.begin() + dst,
                      rotation.steps.begin() + src,
                      rotation.steps.begin() + dst + 1);
        }
        rotation.steps[dst] = std::move(item);
        selected_step_ = static_cast<int>(dst);
        step_move_src_ = -1;
        step_move_dst_ = -1;
        history_.Push(cfg);
    }
}

void EditorComponents::DrawBackgroundImage(
    overlay::core::ConfigManager* config_manager,
    overlay::core::TeamRotation& rotation) {
    ImGui::TextUnformatted("\xe8\x83\x8c\xe6\x99\xaf\xe5\x9b\xbe\xe7\x89\x87\xe8\xb7\xaf\xe5\xbe\x84");

    std::string path = rotation.background_image;
    if (path.size() >= sizeof(bg_path_buffer_)) path.resize(sizeof(bg_path_buffer_) - 1);
    std::fill(bg_path_buffer_, bg_path_buffer_ + sizeof(bg_path_buffer_), '\0');
    std::memcpy(bg_path_buffer_, path.data(), path.size());
    if (ImGui::InputText("##bg_path", bg_path_buffer_, sizeof(bg_path_buffer_))) {
        rotation.background_image = bg_path_buffer_;
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) history_.Push(config_manager->GetConfig());

    ImGui::SameLine();
    if (ImGui::Button("\xe9\x80\x89\xe6\x8b\xa9\xe5\x9b\xbe\xe7\x89\x87...")) {
        wchar_t buffer[MAX_PATH] = {};
        OPENFILENAMEW ofn{};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = nullptr;
        ofn.lpstrFile = buffer;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrFilter = L"Image files (*.png;*.jpg;*.jpeg;*.bmp)\0*.png;*.jpg;*.jpeg;*.bmp\0All files (*.*)\0*.*\0\0";
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
        if (GetOpenFileNameW(&ofn)) {
            fs::path src(buffer);
            if (fs::exists(src)) {
                fs::path assets_dir = config_manager->GetExeDirectory() / "assets";
                fs::create_directories(assets_dir);
                fs::path dst = assets_dir / src.filename();
                if (fs::exists(dst)) {
                    fs::path stem = src.stem();
                    fs::path ext = src.extension();
                    for (int n = 1; n < 1000; ++n) {
                        fs::path candidate = assets_dir /
                            std::format("{}_{}{}", stem.string(), n, ext.string());
                        if (!fs::exists(candidate)) {
                            dst = candidate;
                            break;
                        }
                    }
                }
                try {
                    fs::copy_file(src, dst, fs::copy_options::overwrite_existing);
                    fs::path rel = fs::relative(dst, config_manager->GetExeDirectory());
                    rotation.background_image = rel.generic_string();
                    history_.Push(config_manager->GetConfig());
                    LOG_INFO(std::format("Copied background image to {}",
                                         rel.generic_string()));
                } catch (const std::exception& e) {
                    LOG_ERROR(std::format("Failed to copy background image: {}", e.what()));
                }
            }
        }
    }

    if (!rotation.background_image.empty()) {
        fs::path abs_path = overlay::utils::ResourceLoader::ResolvePath(
            config_manager->GetExeDirectory(), rotation.background_image);
        UpdateBackgroundPreview(abs_path, device_);
        if (preview_bg_srv_) {
            ImGui::Text("\xe9\xa2\x84\xe8\xa7\x88 (%ux%u)", preview_bg_width_,
                        preview_bg_height_);
            float max_w = ImGui::GetContentRegionAvail().x;
            if (max_w < 64.0f) max_w = 200.0f;
            float aspect = preview_bg_height_
                               ? static_cast<float>(preview_bg_width_) /
                                     preview_bg_height_
                               : 1.0f;
            float w = max_w;
            float h = w / aspect;
            if (h > 300.0f) {
                h = 300.0f;
                w = h * aspect;
            }
            ImGui::Image(reinterpret_cast<ImTextureID>(preview_bg_srv_.Get()),
                         ImVec2(w, h));
        } else {
            ImGui::TextDisabled("\xe6\x97\xa0\xe6\xb3\x95\xe5\x8a\xa0\xe8\xbd\xbd\xe9\xa2\x84\xe8\xa7\x88");
        }
    }
}

void EditorComponents::UpdateBackgroundPreview(const fs::path& abs_path,
                                               ID3D11Device* device) {
    if (!device) return;
    std::string key = abs_path.string();
    if (key == preview_bg_path_ && preview_bg_srv_) return;

    preview_bg_srv_.Reset();
    preview_bg_width_ = 0;
    preview_bg_height_ = 0;
    preview_bg_path_ = key;

    auto img = resource_loader_.LoadImage(abs_path);
    if (!img) return;

    ID3D11ShaderResourceView* srv = nullptr;
    if (CreatePreviewTexture(*img, device, &srv, preview_bg_width_,
                             preview_bg_height_)) {
        preview_bg_srv_.Attach(srv);
    }
}

bool EditorComponents::CreatePreviewTexture(const overlay::utils::ImageData& data,
                                            ID3D11Device* device,
                                            ID3D11ShaderResourceView** srv_out,
                                            UINT& width_out,
                                            UINT& height_out) {
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = data.width;
    desc.Height = data.height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA init{};
    init.pSysMem = data.pixels.data();
    init.SysMemPitch = data.pitch;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
    HRESULT hr = device->CreateTexture2D(&desc, &init, tex.GetAddressOf());
    if (FAILED(hr)) {
        LOG_ERROR(std::format("CreatePreviewTexture CreateTexture2D failed: 0x{:08X}",
                              static_cast<uint32_t>(hr)));
        return false;
    }
    hr = device->CreateShaderResourceView(tex.Get(), nullptr, srv_out);
    if (FAILED(hr)) {
        LOG_ERROR(std::format("CreatePreviewTexture CreateShaderResourceView failed: 0x{:08X}",
                              static_cast<uint32_t>(hr)));
        return false;
    }
    width_out = data.width;
    height_out = data.height;
    return true;
}

void EditorComponents::DrawSettings(overlay::core::AppConfig& cfg) {
    using json = nlohmann::json;
    auto ensure_object = [](json& parent, const std::string& key) -> json& {
        if (!parent.contains(key) || !parent[key].is_object()) {
            parent[key] = json::object();
        }
        return parent[key];
    };

    json& display = ensure_object(cfg.settings, "display");
    json& input = ensure_object(cfg.settings, "input");
    json& hotkeys = ensure_object(cfg.settings, "hotkeys");

    auto commit = [this, &cfg]() {
        if (ImGui::IsItemDeactivatedAfterEdit())
            history_.Push(cfg);
    };

    if (ImGui::CollapsingHeader("\xe6\x98\xbe\xe7\xa4\xba",
                                ImGuiTreeNodeFlags_DefaultOpen)) {
        float opacity = display.value("opacity", 0.85f);
        if (ImGui::SliderFloat("\xe4\xb8\x8d\xe9\x80\x8f\xe6\x98\x8e\xe5\xba\xa6", &opacity,
                               0.1f, 1.0f)) {
            display["opacity"] = opacity;
        }
        commit();

        float scale = display.value("scale", 1.0f);
        if (ImGui::SliderFloat("\xe7\xbc\xa9\xe6\x94\xbe", &scale, 0.5f, 2.0f)) {
            display["scale"] = scale;
        }
        commit();

        json& pos = ensure_object(display, "position");
        int x = pos.value("x", 100);
        int y = pos.value("y", 100);
        int xy[2] = {x, y};
        if (ImGui::InputInt2("\xe7\xaa\x97\xe5\x8f\xa3\xe4\xbd\x8d\xe7\xbd\xae", xy)) {
            pos["x"] = xy[0];
            pos["y"] = xy[1];
        }
        commit();

        bool loop = display.value("loop", true);
        if (ImGui::Checkbox("\xe5\xbe\xaa\xe7\x8e\xaf\xe6\x92\xad\xe6\x94\xbe", &loop)) {
            display["loop"] = loop;
        }
        commit();

        bool show_avatar = display.value("show_avatar", true);
        if (ImGui::Checkbox("\xe6\x98\xbe\xe7\xa4\xba\xe5\xa4\xb4\xe5\x83\x8f", &show_avatar)) {
            display["show_avatar"] = show_avatar;
        }
        commit();
        ImGui::SameLine();
        bool show_stage_bar = display.value("show_stage_bar", true);
        if (ImGui::Checkbox("\xe6\x98\xbe\xe7\xa4\xba\xe9\x98\xb6\xe6\xae\xb5\xe6\x9d\xa1",
                            &show_stage_bar)) {
            display["show_stage_bar"] = show_stage_bar;
        }
        commit();
        ImGui::SameLine();
        bool show_progress = display.value("show_progress", true);
        if (ImGui::Checkbox("\xe6\x98\xbe\xe7\xa4\xba\xe8\xbf\x9b\xe5\xba\xa6\xe6\x9d\xa1",
                            &show_progress)) {
            display["show_progress"] = show_progress;
        }
        commit();
        ImGui::SameLine();
        bool show_arrows = display.value("show_arrows", true);
        if (ImGui::Checkbox("\xe6\x98\xbe\xe7\xa4\xba\xe7\xae\xad\xe5\xa4\xb4", &show_arrows)) {
            display["show_arrows"] = show_arrows;
        }
        commit();

        int avatar_size = display.value("avatar_size", 56);
        if (ImGui::InputInt("\xe5\xa4\xb4\xe5\x83\x8f\xe5\xa4\xa7\xe5\xb0\x8f", &avatar_size)) {
            display["avatar_size"] = std::max(avatar_size, 16);
        }
        commit();
        int stage_bar_height = display.value("stage_bar_height", 28);
        if (ImGui::InputInt("\xe9\x98\xb6\xe6\xae\xb5\xe6\x9d\xa1\xe9\xab\x98\xe5\xba\xa6",
                            &stage_bar_height)) {
            display["stage_bar_height"] = std::max(stage_bar_height, 8);
        }
        commit();
    }

    if (ImGui::CollapsingHeader("\xe6\x8c\x89\xe9\x94\xae\xe6\xa0\xb7\xe5\xbc\x8f")) {
        json& ks = ensure_object(display, "key_style");
        int kw = ks.value("key_width", 90);
        int kh = ks.value("key_height", 44);
        int ksp = ks.value("spacing", 10);
        int kr = ks.value("border_radius", 8);
        if (ImGui::InputInt("\xe5\xae\xbd\xe5\xba\xa6", &kw)) ks["key_width"] = std::max(kw, 20);
        commit();
        if (ImGui::InputInt("\xe9\xab\x98\xe5\xba\xa6", &kh)) ks["key_height"] = std::max(kh, 20);
        commit();
        if (ImGui::InputInt("\xe9\x97\xb4\xe8\xb7\x9d", &ksp)) ks["spacing"] = std::max(ksp, 0);
        commit();
        if (ImGui::InputInt("\xe5\x9c\x86\xe8\xa7\x92", &kr)) ks["border_radius"] = std::max(kr, 0);
        commit();

        auto edit_color = [this, &ks, &cfg](const char* label, const std::string& key,
                                const char* def) {
            ImVec4 col = HexToColor(ks.value(key, std::string(def)));
            if (ImGui::ColorEdit3(label, &col.x)) ks[key] = ColorToHex(col);
            if (ImGui::IsItemDeactivatedAfterEdit()) history_.Push(cfg);
        };
        edit_color("\xe5\xbd\x93\xe5\x89\x8d\xe6\xad\xa5\xe9\xaa\xa4\xe8\x83\x8c\xe6\x99\xaf",
                   "active_color", "#F3F4F6");
        edit_color("\xe5\xb7\xb2\xe5\xae\x8c\xe6\x88\x90\xe8\x83\x8c\xe6\x99\xaf", "done_color",
                   "#4B5563");
        edit_color("\xe6\x9c\xaa\xe5\x88\xb0\xe8\xbe\xbe\xe8\x83\x8c\xe6\x99\xaf",
                   "pending_color", "#1F2937");
        edit_color("\xe5\xbd\x93\xe5\x89\x8d\xe6\xad\xa5\xe9\xaa\xa4\xe6\x96\x87\xe5\xad\x97",
                   "active_text_color", "#111827");
        edit_color("\xe6\x99\xae\xe9\x80\x9a\xe6\x96\x87\xe5\xad\x97", "text_color",
                   "#F3F4F6");
        edit_color("\xe7\xae\xad\xe5\xa4\xb4\xe9\xa2\x9c\xe8\x89\xb2", "arrow_color",
                   "#9CA3AF");
    }

    if (ImGui::CollapsingHeader("\xe7\x83\xad\xe9\x94\xae")) {
        static constexpr std::pair<const char*, const char*> kActions[] = {
            {"toggle_visibility", "\xe6\x98\xbe\xe7\xa4\xba/\xe9\x9a\x90\xe8\x97\x8f"},
            {"play_pause", "\xe6\x92\xad\xe6\x94\xbe/\xe6\x9a\x82\xe5\x81\x9c"},
            {"toggle_mode", "\xe5\x88\x87\xe6\x8d\xa2\xe6\xa8\xa1\xe5\xbc\x8f"},
            {"next_rotation", "\xe4\xb8\x8b\xe4\xb8\x80\xe4\xb8\xaa\xe6\xb5\x81\xe7\xa8\x8b"},
            {"open_editor", "\xe6\x89\x93\xe5\xbc\x80\xe7\xbc\x96\xe8\xbe\x91\xe5\x99\xa8"},
            {"move_mode", "\xe7\xa7\xbb\xe5\x8a\xa8\xe6\xa8\xa1\xe5\xbc\x8f"},
            {"reload_config", "\xe9\x87\x8d\xe8\xbd\xbd\xe9\x85\x8d\xe7\xbd\xae"},
            {"quit", "\xe9\x80\x80\xe5\x87\xba"},
        };
        for (auto [name, label] : kActions) {
            std::string val = hotkeys.value(name, "");
            val.resize(64, '\0');
            if (ImGui::InputText(label, val.data(), val.size())) {
                hotkeys[name] = val.c_str();
            }
            commit();
        }
    }

    if (ImGui::CollapsingHeader("\xe8\xbe\x93\xe5\x85\xa5\xe6\xa3\x80\xe6\xb5\x8b")) {
        int poll_hz = input.value("poll_hz", 60);
        if (ImGui::SliderInt("\xe8\xbd\xae\xe8\xaf\xa2\xe9\xa2\x91\xe7\x8e\x87(Hz)", &poll_hz,
                             30, 120)) {
            input["poll_hz"] = poll_hz;
        }
        commit();

        bool foreground_only = input.value("foreground_only", true);
        if (ImGui::Checkbox("\xe4\xbb\x85\xe6\xb8\xb8\xe6\x88\x8f\xe5\x89\x8d\xe5\x8f\xb0\xe6\x97\xb6\xe6\xa3\x80\xe6\xb5\x8b",
                            &foreground_only)) {
            input["foreground_only"] = foreground_only;
        }
        commit();

        std::string target = input.value("target_process",
                                         std::string("Client-Win64-Shipping.exe"));
        target.resize(128, '\0');
        if (ImGui::InputText("\xe7\x9b\xae\xe6\xa0\x87\xe8\xbf\x9b\xe7\xa8\x8b\xe5\x90\x8d",
                             target.data(), target.size())) {
            input["target_process"] = target.c_str();
        }
        commit();

        bool wrong_flash = input.value("wrong_key_flash", true);
        if (ImGui::Checkbox("\xe9\x94\x99\xe9\x94\xae\xe7\xba\xa2\xe9\x97\xaa", &wrong_flash)) {
            input["wrong_key_flash"] = wrong_flash;
        }
        commit();

        int timeout = input.value("timeout_skip_ms", 0);
        if (ImGui::InputInt("\xe8\xb6\x85\xe6\x97\xb6\xe8\x87\xaa\xe5\x8a\xa8\xe8\xb7\xb3\xe8\xbf\x87(ms, 0=\xe7\xa6\x81\xe7\x94\xa8)",
                            &timeout)) {
            input["timeout_skip_ms"] = std::max(timeout, 0);
        }
        commit();
    }
}

std::vector<EditorComponents::Segment> EditorComponents::BuildSegments(
    const overlay::core::TeamRotation& rotation) {
    std::vector<Segment> segments;
    for (size_t i = 0; i < rotation.steps.size();) {
        size_t j = i;
        while (j < rotation.steps.size() &&
               rotation.steps[j].character == rotation.steps[i].character) {
            ++j;
        }
        segments.push_back({i, j - i, rotation.steps[i].character});
        i = j;
    }
    return segments;
}

void EditorComponents::RebuildStepsFromSegments(
    std::vector<overlay::core::KeyStep>& steps,
    const std::vector<Segment>& segments) {
    std::vector<overlay::core::KeyStep> new_steps;
    new_steps.reserve(steps.size());
    for (const auto& seg : segments) {
        for (size_t i = 0; i < seg.count; ++i) {
            new_steps.push_back(steps[seg.start + i]);
        }
    }
    steps = std::move(new_steps);
}

ImVec4 EditorComponents::HexToColor(const std::string& hex) {
    if (hex.size() >= 7 && hex[0] == '#') {
        unsigned int rgb = 0;
        auto [ptr, ec] = std::from_chars(hex.data() + 1, hex.data() + hex.size(),
                                         rgb, 16);
        if (ec == std::errc{}) {
            return ImVec4(((rgb >> 16) & 0xFF) / 255.0f,
                          ((rgb >> 8) & 0xFF) / 255.0f,
                          (rgb & 0xFF) / 255.0f, 1.0f);
        }
    }
    return ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
}

std::string EditorComponents::ColorToHex(const ImVec4& color) {
    int r = static_cast<int>(std::clamp(color.x * 255.0f, 0.0f, 255.0f));
    int g = static_cast<int>(std::clamp(color.y * 255.0f, 0.0f, 255.0f));
    int b = static_cast<int>(std::clamp(color.z * 255.0f, 0.0f, 255.0f));
    char buf[8];
    std::snprintf(buf, sizeof(buf), "#%02X%02X%02X", r, g, b);
    return buf;
}

bool EditorComponents::IsCharacterUsed(
    const overlay::core::TeamRotation& rotation, std::string_view name) {
    for (const auto& step : rotation.steps) {
        if (step.character == name) return true;
    }
    return false;
}

std::string EditorComponents::UniqueRotationName(
    const overlay::core::AppConfig& cfg, const std::string& base) {
    for (int n = 1;; ++n) {
        std::string name = std::format("{} {}", base, n);
        bool exists = false;
        for (const auto& rot : cfg.rotations) {
            if (rot.name == name) {
                exists = true;
                break;
            }
        }
        if (!exists) return name;
    }
}

} // namespace overlay::editor
