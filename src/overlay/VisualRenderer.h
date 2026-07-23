#pragma once

#include "overlay/IRenderer.h"
#include "core/TeamRotation.h"

#include <string>

namespace overlay::utils {
struct ImageData;
}

namespace overlay::overlay {

struct RenderState {
    size_t current_step = 0;
    float scale = 1.0f;
    float opacity = 1.0f;
    bool move_mode = false;
    float wrong_key_flash = 0.0f; // 0..1 progress
    float window_width = 0;
    float window_height = 0;
    const ::overlay::utils::ImageData* bg_image = nullptr;
    const ::overlay::utils::ImageData* avatar_image = nullptr;
};

class VisualRenderer {
public:
    explicit VisualRenderer(IRenderer* renderer);
    ~VisualRenderer();

    void SetRotation(const ::overlay::core::TeamRotation* rotation);

    // Computes desired window size for the current rotation and scale.
    void ComputeWindowSize(float scale, float& out_width, float& out_height) const;

    void Render(const RenderState& state);

private:
    void EnsureTextFormats();
    void UpdateBitmaps(const RenderState& state);
    void ReleaseResources();

    Color ParseHexColor(std::string_view hex, float alpha = 1.0f) const;
    Color GetCharacterThemeColor(const ::overlay::core::CharacterInfo* character) const;
    const ::overlay::core::CharacterInfo* GetCurrentCharacter(size_t step_index) const;
    const ::overlay::core::StageMarker* GetCurrentStage(size_t step_index) const;

    void DrawBackground(const RenderState& state, const Color& theme_color);
    void DrawBorderAndGlow(const RenderState& state, const Color& theme_color);
    void DrawStageBar(const RenderState& state, const Color& theme_color);
    void DrawAvatar(const RenderState& state, const Color& theme_color);
    void DrawKeySequence(const RenderState& state, const Color& theme_color);
    void DrawKeyIcon(float x, float y, float size, const std::string& key_icon,
                     const std::string& key, const Color& text_color);

    IRenderer* renderer_ = nullptr;
    const ::overlay::core::TeamRotation* rotation_ = nullptr;

    TextFormatHandle fmt_normal_ = 0;
    TextFormatHandle fmt_bold_ = 0;
    TextFormatHandle fmt_small_ = 0;

    BitmapHandle bg_bitmap_ = 0;
    BitmapHandle avatar_bitmap_ = 0;

    const void* cached_bg_ptr_ = nullptr;
    const void* cached_avatar_ptr_ = nullptr;
};

} // namespace overlay::overlay
