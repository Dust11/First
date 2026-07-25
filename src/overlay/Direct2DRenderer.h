#pragma once

#include "overlay/IRenderer.h"

#include <d2d1_2.h>
#include <dcomp.h>
#include <dwrite.h>
#include <dxgi1_2.h>
#include <d3d11_1.h>
#include <wrl/client.h>

#include <windows.h>

#include <vector>

namespace overlay::overlay {

class Direct2DRenderer : public IRenderer {
public:
    Direct2DRenderer();
    ~Direct2DRenderer() override;

    Direct2DRenderer(const Direct2DRenderer&) = delete;
    Direct2DRenderer& operator=(const Direct2DRenderer&) = delete;

    bool Initialize(HWND hwnd);
    void Shutdown();

    // Device lost detection and recovery
    bool CheckDeviceLost();
    bool RecreateDeviceResources();

    // Render frame
    void BeginDraw();
    void EndDraw();
    void Present();

    void Resize(int width, int height);
    void SetVisible(bool visible);

    ID2D1DeviceContext* GetContext() const { return d2d_context_.Get(); }
    ID3D11Device* GetD3DDevice() const { return d3d_device_.Get(); }

    // --- IRenderer implementation ---
    void Clear(const Color& color) override;
    void PushAxisAlignedClip(const Rect& rect) override;
    void PopAxisAlignedClip() override;
    void PushLayer(const Ellipse& clip_ellipse) override;
    void PopLayer() override;
    void FillRect(const Rect& rect, const Color& color) override;
    void DrawRect(const Rect& rect, const Color& color, float stroke_width) override;
    void FillRoundedRect(const RoundedRect& rr, const Color& color) override;
    void DrawRoundedRect(const RoundedRect& rr, const Color& color, float stroke_width) override;
    void FillEllipse(const Ellipse& ellipse, const Color& color) override;
    void DrawEllipse(const Ellipse& ellipse, const Color& color, float stroke_width) override;
    void DrawLine(const Point& p0, const Point& p1, const Color& color, float stroke_width) override;
    TextFormatHandle CreateTextFormat(const std::wstring& font_family, float font_size, bool bold) override;
    void ReleaseTextFormat(TextFormatHandle handle) override;
    void DrawString(TextFormatHandle format, const wchar_t* text, size_t length,
                  const Rect& rect, const Color& color, bool hcenter, bool vcenter) override;
    BitmapHandle CreateBitmapFromImageData(const ::overlay::utils::ImageData& image_data) override;
    void ReleaseBitmap(BitmapHandle handle) override;
    void DrawBitmap(BitmapHandle handle, const Rect& dest_rect, float opacity) override;
    void DrawBitmapHighQuality(BitmapHandle handle, const Rect& dest_rect, float opacity) override;
    void* GetRawContext() override { return d2d_context_.Get(); }

private:
    bool CreateD3DDevice();
    bool CreateSwapChain();
    bool CreateD2DResources();
    bool CreateDCompResources();
    bool CreateRenderTargetBitmap();
    bool CreateDWriteFactory();
    void ReleaseResources();

    bool UpdateCompositionClip();

    IDWriteTextFormat* GetTextFormat(TextFormatHandle handle) const;
    ID2D1Bitmap* GetBitmap(BitmapHandle handle) const;

    HWND hwnd_ = nullptr;
    int width_ = 0;
    int height_ = 0;
    bool visible_ = true;

    // Atomic size swap chain: created at max display resolution, clip controls visible area
    int max_width_ = 0;
    int max_height_ = 0;

    Microsoft::WRL::ComPtr<ID3D11Device> d3d_device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3d_context_;
    Microsoft::WRL::ComPtr<IDXGIDevice1> dxgi_device_;
    Microsoft::WRL::ComPtr<IDXGIAdapter> dxgi_adapter_;
    Microsoft::WRL::ComPtr<IDXGIFactory2> dxgi_factory_;

    Microsoft::WRL::ComPtr<IDXGISwapChain2> swap_chain_;

    Microsoft::WRL::ComPtr<ID2D1Factory2> d2d_factory_;
    Microsoft::WRL::ComPtr<ID2D1Device1> d2d_device_;
    Microsoft::WRL::ComPtr<ID2D1DeviceContext> d2d_context_;
    Microsoft::WRL::ComPtr<ID2D1Bitmap1> d2d_target_bitmap_;
    Microsoft::WRL::ComPtr<ID2D1Layer> d2d_layer_;

    Microsoft::WRL::ComPtr<IDCompositionDevice> dcomp_device_;
    Microsoft::WRL::ComPtr<IDCompositionTarget> dcomp_target_;
    Microsoft::WRL::ComPtr<IDCompositionVisual> dcomp_visual_;

    Microsoft::WRL::ComPtr<IDWriteFactory> dwrite_factory_;

    struct TextFormatSlot {
        Microsoft::WRL::ComPtr<IDWriteTextFormat> format;
    };
    struct BitmapSlot {
        Microsoft::WRL::ComPtr<ID2D1Bitmap> bitmap;
    };
    std::vector<TextFormatSlot> text_formats_;
    std::vector<BitmapSlot> bitmaps_;
};

} // namespace overlay::overlay
