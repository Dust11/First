#include "editor/EditorComponents.h"

#include "utils/Logger.h"

#include <algorithm>
#include <charconv>
#include <cstdio>
#include <cstring>
#include <format>

namespace overlay::editor {

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

void EditorComponents::Draw(overlay::core::ConfigManager* config_manager,
                            const std::function<void()>& apply_callback) {
    if (!config_manager) return;

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

    auto& rotation = cfg.rotations[selected_rotation_];

    ImGui::Begin("\xe9\x98\x9f\xe4\xbc\x8d\xe6\xb5\x81\xe7\xa8\x8b\xe7\xbc\x96\xe8\xbe\x91\xe5\x99\xa8"); // 队伍流程编辑器
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
        DrawSegments(rotation);

        ImGui::TableNextColumn();
        DrawCharacters(rotation);

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
        DrawStages(rotation);

        ImGui::TableNextColumn();
        DrawKeySequence(rotation);

        ImGui::EndTable();
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
    }
    ImGui::SameLine();
    if (ImGui::Button("\xe8\xae\xbe\xe4\xb8\xba\xe5\xbd\x93\xe5\x89\x8d") && selected_rotation_ >= 0) { // 设为当前
        cfg.active_rotation = cfg.rotations[selected_rotation_].name;
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

void EditorComponents::DrawSegments(overlay::core::TeamRotation& rotation) {
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
                }
            }
            ImGui::EndDragDropTarget();
        }
        ImGui::PopID();
    }
    ImGui::EndChild();
}

void EditorComponents::DrawCharacters(overlay::core::TeamRotation& rotation) {
    ImGui::TextUnformatted("\xe8\xa7\x92\xe8\x89\xb2\xe8\xa7\x86\xe8\xa7\x89\xe4\xbf\xa1\xe6\x81\xaf"); // 角色视觉信息

    if (ImGui::Button("\xe6\xb7\xbb\xe5\x8a\xa0\xe8\xa7\x92\xe8\x89\xb2")) { // 添加角色
        std::string name = new_character_name_;
        if (!name.empty() && !overlay::core::FindCharacter(rotation, name)) {
            rotation.characters.push_back({name, "", "#888888"});
            selected_character_ = static_cast<int>(rotation.characters.size()) - 1;
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

        std::string avatar = c.avatar_image;
        avatar.resize(512);
        if (ImGui::InputText("\xe5\xa4\xb4\xe5\x83\x8f\xe8\xb7\xaf\xe5\xbe\x84", avatar.data(), // 头像路径
                             avatar.size())) {
            c.avatar_image = avatar.c_str();
        }

        ImVec4 color = HexToColor(c.theme_color);
        if (ImGui::ColorEdit3("\xe4\xb8\xbb\xe9\xa2\x98\xe8\x89\xb2", &color.x)) { // 主题色
            c.theme_color = ColorToHex(color);
        }
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
            } else {
                LOG_WARN(std::format("Cannot delete character '{}': still in use.",
                                     c.name));
            }
        }
    }
}

void EditorComponents::DrawStages(overlay::core::TeamRotation& rotation) {
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

        // 颜色覆盖
        bool auto_color = stage.color.empty();
        if (ImGui::Checkbox("\xe8\x87\xaa\xe5\x8a\xa8\xe9\xa2\x9c\xe8\x89\xb2", &auto_color)) { // 自动颜色
            if (auto_color) {
                stage.color.clear();
            } else {
                stage.color = "#888888";
            }
        }
        if (!auto_color) {
            ImVec4 col = HexToColor(stage.color);
            if (ImGui::ColorEdit3("\xe9\xa2\x9c\xe8\x89\xb2", &col.x)) { // 颜色
                stage.color = ColorToHex(col);
            }
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
        }
        stage_to_delete_ = -1;
    }
}

void EditorComponents::DrawKeySequence(overlay::core::TeamRotation& rotation) {
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

                // 技能名
                ImGui::TableSetColumnIndex(3);
                std::string skill = step.skill_name;
                skill.resize(128);
                if (ImGui::InputText("##skill", skill.data(), skill.size())) {
                    step.skill_name = skill.c_str();
                }

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
                ImGui::EndDisabled();

                // 持续时间
                ImGui::TableSetColumnIndex(6);
                int duration = step.duration_ms;
                if (ImGui::InputInt("##dur", &duration)) {
                    step.duration_ms = std::max(duration, 100);
                }

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
    }

    if (step_to_duplicate_ >= 0 &&
        step_to_duplicate_ < static_cast<int>(rotation.steps.size())) {
        const auto& src = rotation.steps[step_to_duplicate_];
        rotation.steps.insert(rotation.steps.begin() + step_to_duplicate_ + 1, src);
        if (selected_step_ == step_to_duplicate_) selected_step_ = step_to_duplicate_ + 1;
        else if (selected_step_ > step_to_duplicate_) ++selected_step_;
        step_to_duplicate_ = -1;
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
