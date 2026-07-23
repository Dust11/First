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
        DrawSegments(cfg.rotations[selected_rotation_]);

        ImGui::TableNextColumn();
        DrawCharacters(cfg.rotations[selected_rotation_]);

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
