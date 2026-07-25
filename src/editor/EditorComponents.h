#pragma once

#include "core/ConfigManager.h"
#include "core/TeamRotation.h"
#include "editor/EditHistory.h"
#include "utils/ResourceLoader.h"

#include "imgui.h"

#include <d3d11.h>
#include <wrl/client.h>

#include <functional>
#include <string_view>
#include <string>
#include <unordered_map>
#include <vector>

namespace overlay::editor {

// 编辑器子组件：队伍流程管理、角色段编排、角色视觉信息、阶段标记、按键序列编辑
class EditorComponents {
public:
    EditorComponents() = default;

    // 绘制完整编辑器 UI；由 EditorWindow::RenderFrame 每帧调用
    void Draw(overlay::core::ConfigManager* config_manager,
              ID3D11Device* device,
              const std::function<void()>& apply_callback);

    // 编辑器关闭时清空历史（不跨会话持久化）
    void OnEditorHidden();

private:
    struct Segment {
        size_t start = 0;
        size_t count = 0;
        std::string character;
    };

    void DrawRotations(overlay::core::AppConfig& cfg,
                       overlay::core::ConfigManager* config_manager,
                       const std::function<void()>& apply_callback);
    void DrawSegments(overlay::core::AppConfig& cfg,
                      overlay::core::TeamRotation& rotation);
    void DrawCharacters(overlay::core::AppConfig& cfg,
                        overlay::core::TeamRotation& rotation);
    void DrawStages(overlay::core::AppConfig& cfg,
                    overlay::core::TeamRotation& rotation);
    void DrawKeySequence(overlay::core::AppConfig& cfg,
                         overlay::core::TeamRotation& rotation);
    void DrawBackgroundImage(overlay::core::ConfigManager* config_manager,
                             overlay::core::TeamRotation& rotation);
    void DrawSettings(overlay::core::AppConfig& cfg);

    void ClearTransientState();
    void UpdateBackgroundPreview(const std::filesystem::path& abs_path,
                                 ID3D11Device* device);
    bool CreatePreviewTexture(const overlay::utils::ImageData& data,
                              ID3D11Device* device,
                              ID3D11ShaderResourceView** srv_out,
                              UINT& width_out,
                              UINT& height_out);

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
    int selected_stage_ = -1;
    int selected_step_ = -1;

    bool renaming_ = false;
    int rename_index_ = -1;
    char rename_buffer_[256]{};

    char new_rotation_name_[256] = "\xe6\x96\xb0\xe6\xb5\x81\xe7\xa8\x8b 1"; // "新流程 1"
    char new_character_name_[256] = "\xe6\x96\xb0\xe8\xa7\x92\xe8\x89\xb2"; // "新角色"
    char new_segment_character_[256]{};
    bool show_new_segment_popup_ = false;

    // 背景图预览
    ID3D11Device* device_ = nullptr;
    overlay::utils::ResourceLoader resource_loader_;
    bool resource_loader_initialized_ = false;
    std::string preview_bg_path_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> preview_bg_srv_;
    UINT preview_bg_width_ = 0;
    UINT preview_bg_height_ = 0;
    char bg_path_buffer_[512]{};

    // 阶段 / 按键序列的延迟操作（避免在 clipper/循环中直接修改容器）
    int stage_to_delete_ = -1;
    int step_to_delete_ = -1;
    int step_to_duplicate_ = -1;
    int step_move_src_ = -1;
    int step_move_dst_ = -1;

    EditHistory history_;
    bool history_seeded_ = false;
};

} // namespace overlay::editor
