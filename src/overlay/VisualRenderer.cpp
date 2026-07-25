#include "overlay/VisualRenderer.h"

#include "utils/TextEncoding.h"
#include "utils/Logger.h"
#include "utils/ResourceLoader.h"

#include <algorithm>
#include <format>

namespace overlay::overlay {

namespace {

// Base layout constants (unscaled)
constexpr float kStageBarHeight = 28.0f;
constexpr float kAvatarSize = 56.0f;
constexpr float kKeyWidth = 90.0f;
constexpr float kKeyHeight = 44.0f;
constexpr float kSpacing = 10.0f;
constexpr float kBorderRadius = 8.0f;
constexpr float kPadding = 12.0f;
constexpr float kAvatarMargin = 12.0f;
constexpr float kKeyIconWidth = 32.0f;
constexpr int kVisibleBefore = 3;
constexpr int kVisibleAfter = 6;
constexpr int kVisibleCount = kVisibleBefore + kVisibleAfter + 1; // 10

// Color constants
constexpr Color kColorBackgroundFallback{10.0f / 255.0f, 10.0f / 255.0f, 18.0f / 255.0f, 0.82f};
constexpr Color kColorBackgroundOutline{1.0f, 1.0f, 1.0f, 0.15f};
constexpr Color kColorKeyActiveBg{243.0f / 255.0f, 244.0f / 255.0f, 246.0f / 255.0f, 1.0f};
constexpr Color kColorKeyDoneBg{75.0f / 255.0f, 85.0f / 255.0f, 99.0f / 255.0f, 1.0f};
constexpr Color kColorKeyPendingBg{31.0f / 255.0f, 41.0f / 255.0f, 55.0f / 255.0f, 1.0f};
constexpr Color kColorKeyStroke{1.0f, 1.0f, 1.0f, 0.10f};
constexpr Color kColorArrow{156.0f / 255.0f, 163.0f / 255.0f, 175.0f / 255.0f, 1.0f};
constexpr Color kColorTextActive{17.0f / 255.0f, 24.0f / 255.0f, 39.0f / 255.0f, 1.0f};
constexpr Color kColorTextLight{243.0f / 255.0f, 244.0f / 255.0f, 246.0f / 255.0f, 1.0f};
constexpr Color kColorTextDim{156.0f / 255.0f, 163.0f / 255.0f, 175.0f / 255.0f, 1.0f};
constexpr Color kColorStageBarBg{10.0f / 255.0f, 10.0f / 255.0f, 18.0f / 255.0f, 0.90f};
constexpr Color kColorMoveModeBorder{1.0f, 1.0f, 0.0f, 1.0f};
constexpr Color kColorWrongFlash{1.0f, 0.0f, 0.0f, 1.0f};

Color WithAlpha(const Color& c, float alpha) {
    return Color{c.r, c.g, c.b, c.a * alpha};
}

} // namespace

VisualRenderer::VisualRenderer(IRenderer* renderer) : renderer_(renderer) {
    EnsureTextFormats();
}

VisualRenderer::~VisualRenderer() {
    ReleaseResources();
}

void VisualRenderer::ReleaseResources() {
    if (renderer_) {
        if (bg_bitmap_) renderer_->ReleaseBitmap(bg_bitmap_);
        if (avatar_bitmap_) renderer_->ReleaseBitmap(avatar_bitmap_);
        if (fmt_normal_) renderer_->ReleaseTextFormat(fmt_normal_);
        if (fmt_bold_) renderer_->ReleaseTextFormat(fmt_bold_);
        if (fmt_small_) renderer_->ReleaseTextFormat(fmt_small_);
    }
    bg_bitmap_ = 0;
    avatar_bitmap_ = 0;
    fmt_normal_ = 0;
    fmt_bold_ = 0;
    fmt_small_ = 0;
}

void VisualRenderer::SetRotation(const ::overlay::core::TeamRotation* rotation) {
    rotation_ = rotation;
}

void VisualRenderer::ComputeWindowSize(float scale, float& out_width, float& out_height) const {
    float s = scale;
    float avatar_area = (kAvatarMargin * 2.0f + kAvatarSize) * s;
    float keys_width = (kVisibleCount * (kKeyWidth + kSpacing) - kSpacing) * s;
    float w = avatar_area + keys_width + kPadding * s;
    float h = (kStageBarHeight + std::max(kAvatarSize, kKeyHeight) + kPadding * 2.0f) * s;
    out_width = w;
    out_height = h;
}

void VisualRenderer::EnsureTextFormats() {
    if (!renderer_ || fmt_normal_) return;
    fmt_normal_ = renderer_->CreateTextFormat(L"Microsoft YaHei UI", 14.0f, false);
    fmt_bold_ = renderer_->CreateTextFormat(L"Microsoft YaHei UI", 14.0f, true);
    fmt_small_ = renderer_->CreateTextFormat(L"Microsoft YaHei UI", 12.0f, false);
}

void VisualRenderer::UpdateBitmaps(const RenderState& state) {
    if (!renderer_) return;

    if (state.bg_image != cached_bg_ptr_) {
        if (bg_bitmap_) {
            renderer_->ReleaseBitmap(bg_bitmap_);
            bg_bitmap_ = 0;
        }
        cached_bg_ptr_ = state.bg_image;
        if (state.bg_image) {
            bg_bitmap_ = renderer_->CreateBitmapFromImageData(*state.bg_image);
        }
    }

    if (state.avatar_image != cached_avatar_ptr_) {
        if (avatar_bitmap_) {
            renderer_->ReleaseBitmap(avatar_bitmap_);
            avatar_bitmap_ = 0;
        }
        cached_avatar_ptr_ = state.avatar_image;
        if (state.avatar_image) {
            avatar_bitmap_ = renderer_->CreateBitmapFromImageData(*state.avatar_image);
        }
    }
}

Color VisualRenderer::ParseHexColor(std::string_view hex, float alpha) const {
    if (hex.empty() || hex[0] != '#' || hex.size() != 7) {
        return Color{1.0f, 1.0f, 1.0f, alpha};
    }
    auto hex_val = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return 0;
    };
    float r = static_cast<float>(hex_val(hex[1]) * 16 + hex_val(hex[2])) / 255.0f;
    float g = static_cast<float>(hex_val(hex[3]) * 16 + hex_val(hex[4])) / 255.0f;
    float b = static_cast<float>(hex_val(hex[5]) * 16 + hex_val(hex[6])) / 255.0f;
    return Color{r, g, b, alpha};
}

Color VisualRenderer::GetCharacterThemeColor(const ::overlay::core::CharacterInfo* character) const {
    if (!character || character->theme_color.empty()) {
        return Color{0.5f, 0.5f, 0.5f, 1.0f};
    }
    return ParseHexColor(character->theme_color);
}

const ::overlay::core::CharacterInfo* VisualRenderer::GetCurrentCharacter(size_t step_index) const {
    if (!rotation_ || step_index >= rotation_->steps.size()) return nullptr;
    return ::overlay::core::FindCharacter(*rotation_, rotation_->steps[step_index].character);
}

const ::overlay::core::StageMarker* VisualRenderer::GetCurrentStage(size_t step_index) const {
    if (!rotation_ || rotation_->stages.empty()) return nullptr;
    const ::overlay::core::StageMarker* result = &rotation_->stages[0];
    for (const auto& stage : rotation_->stages) {
        if (stage.start_step <= step_index) {
            result = &stage;
        } else {
            break;
        }
    }
    return result;
}

void VisualRenderer::Render(const RenderState& state) {
    if (!renderer_) return;

    UpdateBitmaps(state);

    const ::overlay::core::CharacterInfo* current_char = GetCurrentCharacter(state.current_step);
    Color theme_color = GetCharacterThemeColor(current_char);

    // Background (bottom layer)
    DrawBackground(state, theme_color);

    // Border / glow
    DrawBorderAndGlow(state, theme_color);

    // Stage bar
    DrawStageBar(state, theme_color);

    // Progress bar
    DrawProgressBar(state, theme_color);

    // Avatar
    DrawAvatar(state, theme_color);

    // Key sequence
    DrawKeySequence(state, theme_color);
}

void VisualRenderer::DrawBackground(const RenderState& state, const Color& theme_color) {
    float w = state.window_width;
    float h = state.window_height;
    float s = state.scale;
    float br = kBorderRadius * s;

    if (bg_bitmap_ && state.bg_image) {
        // Cover-fit bitmap
        float img_w = static_cast<float>(state.bg_image->width);
        float img_h = static_cast<float>(state.bg_image->height);
        float img_aspect = img_w / img_h;
        float win_aspect = w / h;

        float draw_w, draw_h, draw_x, draw_y;
        if (img_aspect > win_aspect) {
            draw_h = h;
            draw_w = h * img_aspect;
            draw_x = (w - draw_w) * 0.5f;
            draw_y = 0.0f;
        } else {
            draw_w = w;
            draw_h = w / img_aspect;
            draw_x = 0.0f;
            draw_y = (h - draw_h) * 0.5f;
        }
        renderer_->DrawBitmapHighQuality(bg_bitmap_, {draw_x, draw_y, draw_w, draw_h}, state.opacity);

        // Darkening overlay for readability
        renderer_->FillRect({0.0f, 0.0f, w, h}, Color{0.0f, 0.0f, 0.0f, 0.40f * state.opacity});
    } else {
        // Fallback rounded panel
        RoundedRect rr{0.0f, 0.0f, w, h, br, br};
        renderer_->FillRoundedRect(rr, WithAlpha(kColorBackgroundFallback, state.opacity));

        // Outline
        renderer_->DrawRoundedRect(rr, WithAlpha(kColorBackgroundOutline, state.opacity), 1.0f * s);

        // Simple theme glow (expanded rect with low alpha)
        // TODO: replace with Gaussian blur effect for proper glow
        float glow = 4.0f * s;
        RoundedRect glow_rr{-glow, -glow, w + glow * 2.0f, h + glow * 2.0f, br + glow, br + glow};
        Color glow_c = theme_color;
        glow_c.a = 0.15f * state.opacity;
        renderer_->FillRoundedRect(glow_rr, glow_c);
    }
}

void VisualRenderer::DrawBorderAndGlow(const RenderState& state, const Color& /*theme_color*/) {
    float w = state.window_width;
    float h = state.window_height;
    float s = state.scale;
    float br = kBorderRadius * s;

    RoundedRect rr{0.0f, 0.0f, w, h, br, br};
    Color border = state.move_mode ? kColorMoveModeBorder : kColorBackgroundOutline;
    border.a *= state.opacity;

    if (state.move_mode) {
        // Thicker dashed-like border indication
        renderer_->DrawRoundedRect(rr, border, 3.0f * s);
    } else {
        renderer_->DrawRoundedRect(rr, border, 1.0f * s);
    }
}

void VisualRenderer::DrawStageBar(const RenderState& state, const Color& theme_color) {
    if (!rotation_ || rotation_->stages.empty()) return;

    float s = state.scale;
    float w = state.window_width;
    float h = kStageBarHeight * s;

    // Background
    renderer_->FillRect({0.0f, 0.0f, w, h}, WithAlpha(kColorStageBarBg, state.opacity));

    size_t num_stages = rotation_->stages.size();
    float seg_width = w / static_cast<float>(num_stages);

    const ::overlay::core::StageMarker* current_stage = GetCurrentStage(state.current_step);

    for (size_t i = 0; i < num_stages; ++i) {
        const auto& stage = rotation_->stages[i];
        float seg_x = static_cast<float>(i) * seg_width;

        bool is_current = (current_stage && current_stage->start_step == stage.start_step);
        bool is_completed = (current_stage && stage.start_step < current_stage->start_step);

        Color seg_color = theme_color;
        if (!stage.color.empty()) {
            seg_color = ParseHexColor(stage.color);
        }

        if (is_current) {
            // Highlight block for current stage
            renderer_->FillRect({seg_x, 0.0f, seg_width, h}, WithAlpha(seg_color, state.opacity));
        }

        // Separator line
        if (i > 0) {
            renderer_->DrawLine({seg_x, 2.0f * s}, {seg_x, h - 2.0f * s},
                                  Color{1.0f, 1.0f, 1.0f, 0.15f * state.opacity}, 1.0f * s);
        }

        // Label text
        Color text_color;
        if (is_current) {
            text_color = Color{1.0f, 1.0f, 1.0f, state.opacity};
        } else if (is_completed) {
            text_color = Color{0.7f, 0.7f, 0.7f, state.opacity};
        } else {
            text_color = Color{0.4f, 0.4f, 0.4f, state.opacity};
        }

        std::wstring label = ::overlay::utils::Utf8ToWstring(stage.label);
        Rect text_rect{seg_x + 2.0f * s, 0.0f, seg_width - 4.0f * s, h};
        TextFormatHandle fmt = is_current ? fmt_bold_ : fmt_normal_;
        renderer_->DrawString(fmt, label.c_str(), label.size(), text_rect, text_color, true, true);
    }
}

void VisualRenderer::DrawProgressBar(const RenderState& state, const Color& theme_color) {
    if (!state.show_progress) return;

    float s = state.scale;
    float h = 4.0f * s;
    float margin = 8.0f * s;
    float y = (kStageBarHeight * s) + margin;
    float left = margin;
    float right = state.window_width - margin;
    if (right <= left) return;

    RoundedRect track{left, y, right - left, h, h * 0.5f, h * 0.5f};
    // Track: semi-transparent white
    renderer_->FillRoundedRect(track,
        WithAlpha(Color{1.0f, 1.0f, 1.0f, 0.2f}, state.opacity));

    if (state.overall_progress > 0.0f) {
        float fill_w = (right - left) * state.overall_progress;
        RoundedRect fill{left, y, fill_w, h, h * 0.5f, h * 0.5f};
        // Fill: theme color
        renderer_->FillRoundedRect(fill,
            WithAlpha(theme_color, 0.9f * state.opacity));
    }
}

void VisualRenderer::DrawAvatar(const RenderState& state, const Color& theme_color) {
    float s = state.scale;
    float pad = kPadding * s;
    float avatar_sz = kAvatarSize * s;
    float stage_h = kStageBarHeight * s;

    float content_y = stage_h;
    float content_center_y = content_y + (state.window_height - content_y) * 0.5f;
    float avatar_x = pad;
    float avatar_y = content_center_y - avatar_sz * 0.5f;

    Ellipse avatar_ellipse{{avatar_x + avatar_sz * 0.5f, avatar_y + avatar_sz * 0.5f},
                            avatar_sz * 0.5f, avatar_sz * 0.5f};

    // Theme outline (draw slightly larger circle behind)
    float outline_width = 2.5f * s;
    Ellipse outline_ellipse = avatar_ellipse;
    outline_ellipse.radius_x += outline_width * 0.5f;
    outline_ellipse.radius_y += outline_width * 0.5f;
    renderer_->DrawEllipse(outline_ellipse, WithAlpha(theme_color, state.opacity), outline_width);

    // Clip to circle and draw avatar image or fallback
    renderer_->PushLayer(avatar_ellipse);

    if (avatar_bitmap_) {
        renderer_->DrawBitmap(avatar_bitmap_, {avatar_x, avatar_y, avatar_sz, avatar_sz}, state.opacity);
    } else {
        // Fallback: dimmed theme color circle + first character
        Color fallback_bg = theme_color;
        fallback_bg.r *= 0.5f;
        fallback_bg.g *= 0.5f;
        fallback_bg.b *= 0.5f;
        fallback_bg.a = state.opacity;
        renderer_->FillEllipse(avatar_ellipse, fallback_bg);

        const ::overlay::core::CharacterInfo* character = GetCurrentCharacter(state.current_step);
        if (character) {
            std::wstring first = ::overlay::utils::Utf8ToWstring(character->name);
            if (!first.empty()) {
                Rect text_rect{avatar_x, avatar_y, avatar_sz, avatar_sz};
                renderer_->DrawString(fmt_bold_, first.c_str(), 1, text_rect,
                                      Color{1.0f, 1.0f, 1.0f, state.opacity}, true, true);
            }
        }
    }

    renderer_->PopLayer();
}

void VisualRenderer::DrawKeySequence(const RenderState& state, const Color& theme_color) {
    if (!rotation_ || rotation_->steps.empty()) return;

    float s = state.scale;
    float stage_h = kStageBarHeight * s;
    float avatar_sz = kAvatarSize * s;
    float key_w = kKeyWidth * s;
    float key_h = kKeyHeight * s;
    float sp = kSpacing * s;
    float br = kBorderRadius * s;
    float icon_w = kKeyIconWidth * s;

    float content_y = stage_h;
    float content_center_y = content_y + (state.window_height - content_y) * 0.5f;
    float keys_y = content_center_y - key_h * 0.5f;

    float avatar_area_w = kPadding * s + avatar_sz + kPadding * s;
    float keys_start_x = avatar_area_w;

    int current = static_cast<int>(state.current_step);

    for (int i = 0; i < kVisibleCount; ++i) {
        int step_idx = current - kVisibleBefore + i;
        if (step_idx < 0 || step_idx >= static_cast<int>(rotation_->steps.size())) continue;

        const auto& step = rotation_->steps[step_idx];
        float key_x = keys_start_x + static_cast<float>(i) * (key_w + sp);

        bool is_active = (step_idx == current);
        bool is_done = (step_idx < current);

        Color bg_color;
        Color text_color;
        if (is_active) {
            bg_color = kColorKeyActiveBg;
            text_color = kColorTextActive;
        } else if (is_done) {
            bg_color = kColorKeyDoneBg;
            text_color = kColorTextLight;
        } else {
            bg_color = kColorKeyPendingBg;
            text_color = kColorTextDim;
        }

        // Theme glow for active key
        if (is_active) {
            // TODO: replace with Gaussian blur for proper glow
            float glow = 6.0f * s;
            RoundedRect glow_rr{key_x - glow, keys_y - glow,
                                key_w + glow * 2.0f, key_h + glow * 2.0f,
                                br + glow, br + glow};
            Color glow_c = theme_color;
            glow_c.a = 0.25f * state.opacity;
            renderer_->FillRoundedRect(glow_rr, glow_c);
        }

        // Key card background
        RoundedRect key_rr{key_x, keys_y, key_w, key_h, br, br};
        renderer_->FillRoundedRect(key_rr, bg_color);
        renderer_->DrawRoundedRect(key_rr, kColorKeyStroke, 1.0f * s);

        // Wrong key flash overlay
        if (is_active && state.wrong_key_flash > 0.0f) {
            Color flash = kColorWrongFlash;
            flash.a *= state.wrong_key_flash;
            renderer_->FillRoundedRect(key_rr, flash);
        }

        // Icon area (left side)
        float icon_x = key_x + 4.0f * s;
        float icon_y = keys_y + (key_h - icon_w) * 0.5f;
        DrawKeyIcon(icon_x, icon_y, icon_w, step.key_icon, step.key, text_color);

        // Skill name (right of icon)
        float text_x = icon_x + icon_w + 2.0f * s;
        float text_w = key_w - (icon_w + 8.0f * s);
        Rect text_rect{text_x, keys_y, text_w, key_h};
        std::wstring skill = ::overlay::utils::Utf8ToWstring(step.skill_name);
        renderer_->DrawString(fmt_normal_, skill.c_str(), skill.size(), text_rect, text_color, false, true);

        // Arrow after key (except last visible or last step)
        if (i < kVisibleCount - 1 && step_idx + 1 < static_cast<int>(rotation_->steps.size())) {
            float arrow_x = key_x + key_w + sp * 0.5f - 8.0f * s;
            float arrow_y = keys_y + key_h * 0.5f - 6.0f * s;
            Rect arrow_rect{arrow_x, arrow_y, 16.0f * s, 12.0f * s};
            std::wstring arrow = L"\x00BB\x00BB"; // »»
            renderer_->DrawString(fmt_small_, arrow.c_str(), arrow.size(), arrow_rect, kColorArrow, true, true);
        }
    }
}

void VisualRenderer::DrawKeyIcon(float x, float y, float size, const std::string& key_icon,
                                 const std::string& key, const Color& text_color) {
    float s = size / 32.0f; // normalize to base 32px
    float r = 4.0f * s;     // corner radius

    if (key_icon == "mouse_left" || key_icon == "mouse_left_hold") {
        Color yellow{1.0f, 0.85f, 0.2f, 1.0f};
        RoundedRect rr{x, y, size, size, r, r};
        renderer_->FillRoundedRect(rr, yellow);
        renderer_->DrawRoundedRect(rr, Color{0.0f, 0.0f, 0.0f, 0.3f}, 1.0f * s);

        // Left dot
        float dot_r = 3.0f * s;
        renderer_->FillEllipse({{x + size * 0.3f, y + size * 0.5f}, dot_r, dot_r},
                                  Color{0.2f, 0.2f, 0.2f, 1.0f});

        if (key_icon == "mouse_left_hold") {
            // Hold indicator: small bar at bottom
            float bar_w = 8.0f * s;
            float bar_h = 3.0f * s;
            renderer_->FillRect({x + size * 0.5f - bar_w * 0.5f,
                                   y + size - bar_h - 2.0f * s,
                                   bar_w, bar_h},
                                  Color{1.0f, 0.3f, 0.1f, 1.0f});
        }
    } else if (key_icon == "mouse_right") {
        Color red{0.9f, 0.3f, 0.3f, 1.0f};
        RoundedRect rr{x, y, size, size, r, r};
        renderer_->FillRoundedRect(rr, red);
        renderer_->DrawRoundedRect(rr, Color{0.0f, 0.0f, 0.0f, 0.3f}, 1.0f * s);

        // Right dot
        float dot_r = 3.0f * s;
        renderer_->FillEllipse({{x + size * 0.7f, y + size * 0.5f}, dot_r, dot_r},
                                  Color{0.2f, 0.2f, 0.2f, 1.0f});
    } else if (key_icon == "mouse_middle") {
        Color gray{0.6f, 0.6f, 0.6f, 1.0f};
        RoundedRect rr{x, y, size, size, r, r};
        renderer_->FillRoundedRect(rr, gray);
        renderer_->DrawRoundedRect(rr, Color{0.0f, 0.0f, 0.0f, 0.3f}, 1.0f * s);

        // Middle dot
        float dot_r = 3.0f * s;
        renderer_->FillEllipse({{x + size * 0.5f, y + size * 0.5f}, dot_r, dot_r},
                                  Color{0.2f, 0.2f, 0.2f, 1.0f});
    } else {
        // keyboard or fallback (including custom)
        Color key_bg{0.3f, 0.3f, 0.35f, 1.0f};
        RoundedRect rr{x, y, size, size, r, r};
        renderer_->FillRoundedRect(rr, key_bg);
        renderer_->DrawRoundedRect(rr, Color{0.0f, 0.0f, 0.0f, 0.3f}, 1.0f * s);

        // Key letter
        std::wstring key_text = ::overlay::utils::Utf8ToWstring(key);
        if (key_text.size() > 3) key_text.resize(3);
        Rect text_rect{x, y, size, size};
        renderer_->DrawString(fmt_normal_, key_text.c_str(), key_text.size(), text_rect, text_color, true, true);
    }
}

} // namespace overlay::overlay
