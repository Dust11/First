#pragma once

#include "utils/ResourceLoader.h"

#include <cstdint>
#include <string>

namespace overlay::overlay {

struct Color {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 1.0f;
};

struct Point {
    float x = 0.0f;
    float y = 0.0f;
};

struct Rect {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
};

struct RoundedRect : Rect {
    float radius_x = 0.0f;
    float radius_y = 0.0f;
};

struct Ellipse {
    Point center;
    float radius_x = 0.0f;
    float radius_y = 0.0f;
};

using BitmapHandle = uint32_t;
using TextFormatHandle = uint32_t;

class IRenderer {
public:
    virtual ~IRenderer() = default;

    // --- State ---
    virtual void Clear(const Color& color) = 0;
    virtual void PushAxisAlignedClip(const Rect& rect) = 0;
    virtual void PopAxisAlignedClip() = 0;

    // --- Layers (for circular avatar clip) ---
    virtual void PushLayer(const Ellipse& clip_ellipse) = 0;
    virtual void PopLayer() = 0;

    // --- Primitives ---
    virtual void FillRect(const Rect& rect, const Color& color) = 0;
    virtual void DrawRect(const Rect& rect, const Color& color, float stroke_width) = 0;
    virtual void FillRoundedRect(const RoundedRect& rr, const Color& color) = 0;
    virtual void DrawRoundedRect(const RoundedRect& rr, const Color& color, float stroke_width) = 0;
    virtual void FillEllipse(const Ellipse& ellipse, const Color& color) = 0;
    virtual void DrawEllipse(const Ellipse& ellipse, const Color& color, float stroke_width) = 0;
    virtual void DrawLine(const Point& p0, const Point& p1, const Color& color, float stroke_width) = 0;

    // --- Text ---
    virtual TextFormatHandle CreateTextFormat(const std::wstring& font_family, float font_size, bool bold) = 0;
    virtual void ReleaseTextFormat(TextFormatHandle handle) = 0;
    virtual void DrawString(TextFormatHandle format, const wchar_t* text, size_t length,
                          const Rect& rect, const Color& color, bool hcenter, bool vcenter) = 0;

    // --- Bitmap ---
    virtual BitmapHandle CreateBitmapFromImageData(const ::overlay::utils::ImageData& image_data) = 0;
    virtual void ReleaseBitmap(BitmapHandle handle) = 0;
    virtual void DrawBitmap(BitmapHandle handle, const Rect& dest_rect, float opacity) = 0;

    // --- Escape hatch ---
    virtual void* GetRawContext() = 0;
};

} // namespace overlay::overlay
