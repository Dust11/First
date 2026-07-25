#include "overlay/Direct2DRenderer.h"

#include "utils/Logger.h"
#include "utils/ResourceLoader.h"

#include <algorithm>
#include <d2d1effects.h>
#include <format>

// Manually define CLSID_D2D1Scale to avoid initguid.h side effects.
// {9DAF9369-3846-4D0E-A44E-0C607934A5D7}
static const GUID CLSID_D2D1Scale_Local =
    {0x9daf9369, 0x3846, 0x4d0e, {0xa4, 0x4e, 0x0c, 0x60, 0x79, 0x34, 0xa5, 0xd7}};
#define CLSID_D2D1Scale CLSID_D2D1Scale_Local

namespace overlay::overlay {

namespace {

void GetMaxDisplayResolution(int& width, int& height) {
    width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    if (width <= 0) width = 1920;
    if (height <= 0) height = 1080;
}

std::string HResultToString(HRESULT hr) {
    return std::format("HRESULT=0x{:08X}", static_cast<uint32_t>(hr));
}

D2D1_COLOR_F ToD2DColor(const Color& c) {
    return D2D1::ColorF(c.r, c.g, c.b, c.a);
}

D2D1_RECT_F ToD2DRect(const Rect& r) {
    return D2D1::RectF(r.x, r.y, r.x + r.w, r.y + r.h);
}

D2D1_ROUNDED_RECT ToD2DRoundedRect(const RoundedRect& rr) {
    D2D1_RECT_F rect = ToD2DRect(rr);
    return D2D1::RoundedRect(rect, rr.radius_x, rr.radius_y);
}

D2D1_ELLIPSE ToD2DEllipse(const Ellipse& e) {
    return D2D1::Ellipse(D2D1::Point2F(e.center.x, e.center.y), e.radius_x, e.radius_y);
}

} // namespace

Direct2DRenderer::Direct2DRenderer() = default;

Direct2DRenderer::~Direct2DRenderer() {
    Shutdown();
}

bool Direct2DRenderer::Initialize(HWND hwnd) {
    hwnd_ = hwnd;
    GetMaxDisplayResolution(max_width_, max_height_);

    if (!CreateD3DDevice()) return false;
    if (!CreateSwapChain()) return false;
    if (!CreateD2DResources()) return false;
    if (!CreateDWriteFactory()) return false;
    if (!CreateDCompResources()) return false;
    if (!CreateRenderTargetBitmap()) return false;

    LOG_INFO("Direct2DRenderer initialized.");
    return true;
}

void Direct2DRenderer::Shutdown() {
    ReleaseResources();
    hwnd_ = nullptr;
}

void Direct2DRenderer::ReleaseResources() {
    text_formats_.clear();
    bitmaps_.clear();
    dwrite_factory_.Reset();
    d2d_layer_.Reset();
    d2d_target_bitmap_.Reset();
    d2d_context_.Reset();
    d2d_device_.Reset();
    d2d_factory_.Reset();

    if (waitable_object_) {
        CloseHandle(waitable_object_);
        waitable_object_ = nullptr;
    }
    swap_chain_.Reset();

    dcomp_visual_.Reset();
    dcomp_target_.Reset();
    dcomp_device_.Reset();

    dxgi_factory_.Reset();
    dxgi_adapter_.Reset();
    dxgi_device_.Reset();
    d3d_context_.Reset();
    d3d_device_.Reset();
}

bool Direct2DRenderer::CreateD3DDevice() {
    UINT create_flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef _DEBUG
    create_flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL feature_levels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };

    HRESULT hr = D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, create_flags,
        feature_levels, static_cast<UINT>(std::size(feature_levels)),
        D3D11_SDK_VERSION, d3d_device_.GetAddressOf(),
        nullptr, d3d_context_.GetAddressOf());

    if (FAILED(hr)) {
        LOG_ERROR(std::format("D3D11CreateDevice failed: {}", HResultToString(hr)));
        return false;
    }

    hr = d3d_device_.As(&dxgi_device_);
    if (FAILED(hr)) {
        LOG_ERROR(std::format("ID3D11Device::QueryInterface(IDXGIDevice1) failed: {}",
                              HResultToString(hr)));
        return false;
    }

    hr = dxgi_device_->GetAdapter(dxgi_adapter_.GetAddressOf());
    if (FAILED(hr)) {
        LOG_ERROR(std::format("IDXGIDevice1::GetAdapter failed: {}", HResultToString(hr)));
        return false;
    }

    hr = dxgi_adapter_->GetParent(IID_PPV_ARGS(&dxgi_factory_));
    if (FAILED(hr)) {
        LOG_ERROR(std::format("IDXGIAdapter::GetParent(IDXGIFactory2) failed: {}",
                              HResultToString(hr)));
        return false;
    }

    return true;
}

bool Direct2DRenderer::CreateSwapChain() {
    DXGI_SWAP_CHAIN_DESC1 desc{};
    desc.Width = static_cast<UINT>(max_width_);
    desc.Height = static_cast<UINT>(max_height_);
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 2;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    desc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
    desc.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;

    Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain1;
    HRESULT hr = dxgi_factory_->CreateSwapChainForComposition(
        d3d_device_.Get(), &desc, nullptr, swap_chain1.GetAddressOf());
    if (FAILED(hr)) {
        LOG_ERROR(std::format("CreateSwapChainForComposition failed: {}",
                              HResultToString(hr)));
        return false;
    }

    hr = swap_chain1.As(&swap_chain_);
    if (FAILED(hr)) {
        LOG_ERROR(std::format("IDXGISwapChain1::QueryInterface(IDXGISwapChain2) failed: {}",
                              HResultToString(hr)));
        return false;
    }

    hr = swap_chain_->SetMaximumFrameLatency(1);
    if (FAILED(hr)) {
        LOG_WARN(std::format("SetMaximumFrameLatency failed: {}", HResultToString(hr)));
    }

    waitable_object_ = swap_chain_->GetFrameLatencyWaitableObject();
    if (!waitable_object_) {
        LOG_WARN("GetFrameLatencyWaitableObject returned nullptr.");
    }

    return true;
}

bool Direct2DRenderer::CreateD2DResources() {
    D2D1_FACTORY_OPTIONS options{};
#ifdef _DEBUG
    options.debugLevel = D2D1_DEBUG_LEVEL_WARNING;
#endif

    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_MULTI_THREADED, options,
                                   d2d_factory_.GetAddressOf());
    if (FAILED(hr)) {
        LOG_ERROR(std::format("D2D1CreateFactory failed: {}", HResultToString(hr)));
        return false;
    }

    hr = d2d_factory_->CreateDevice(dxgi_device_.Get(), d2d_device_.GetAddressOf());
    if (FAILED(hr)) {
        LOG_ERROR(std::format("ID2D1Factory2::CreateDevice failed: {}",
                              HResultToString(hr)));
        return false;
    }

    hr = d2d_device_->CreateDeviceContext(
        D2D1_DEVICE_CONTEXT_OPTIONS_NONE, d2d_context_.GetAddressOf());
    if (FAILED(hr)) {
        LOG_ERROR(std::format("ID2D1Device1::CreateDeviceContext failed: {}",
                              HResultToString(hr)));
        return false;
    }

    hr = d2d_context_->CreateLayer(nullptr, d2d_layer_.GetAddressOf());
    if (FAILED(hr)) {
        LOG_ERROR(std::format("ID2D1DeviceContext::CreateLayer failed: {}",
                              HResultToString(hr)));
        return false;
    }

    return true;
}

bool Direct2DRenderer::CreateDWriteFactory() {
    HRESULT hr = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(dwrite_factory_.GetAddressOf()));
    if (FAILED(hr)) {
        LOG_ERROR(std::format("DWriteCreateFactory failed: {}", HResultToString(hr)));
        return false;
    }
    return true;
}

bool Direct2DRenderer::CreateDCompResources() {
    HRESULT hr = DCompositionCreateDevice(
        dxgi_device_.Get(), IID_PPV_ARGS(&dcomp_device_));
    if (FAILED(hr)) {
        LOG_ERROR(std::format("DCompositionCreateDevice failed: {}",
                              HResultToString(hr)));
        return false;
    }

    hr = dcomp_device_->CreateTargetForHwnd(hwnd_, true,
                                               dcomp_target_.GetAddressOf());
    if (FAILED(hr)) {
        LOG_ERROR(std::format("IDCompositionDevice::CreateTargetForHwnd failed: {}",
                              HResultToString(hr)));
        return false;
    }

    hr = dcomp_device_->CreateVisual(dcomp_visual_.GetAddressOf());
    if (FAILED(hr)) {
        LOG_ERROR(std::format("IDCompositionDevice::CreateVisual failed: {}",
                              HResultToString(hr)));
        return false;
    }

    hr = dcomp_visual_->SetContent(swap_chain_.Get());
    if (FAILED(hr)) {
        LOG_ERROR(std::format("IDCompositionVisual::SetContent failed: {}",
                              HResultToString(hr)));
        return false;
    }

    hr = dcomp_target_->SetRoot(dcomp_visual_.Get());
    if (FAILED(hr)) {
        LOG_ERROR(std::format("IDCompositionTarget::SetRoot failed: {}",
                              HResultToString(hr)));
        return false;
    }

    if (!UpdateCompositionClip()) return false;

    hr = dcomp_device_->Commit();
    if (FAILED(hr)) {
        LOG_ERROR(std::format("IDCompositionDevice::Commit failed: {}",
                              HResultToString(hr)));
        return false;
    }

    return true;
}

bool Direct2DRenderer::CreateRenderTargetBitmap() {
    Microsoft::WRL::ComPtr<IDXGISurface> surface;
    HRESULT hr = swap_chain_->GetBuffer(0, IID_PPV_ARGS(&surface));
    if (FAILED(hr)) {
        LOG_ERROR(std::format("IDXGISwapChain2::GetBuffer failed: {}",
                              HResultToString(hr)));
        return false;
    }

    D2D1_BITMAP_PROPERTIES1 props{};
    props.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
    props.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
    props.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;

    hr = d2d_context_->CreateBitmapFromDxgiSurface(
        surface.Get(), props, d2d_target_bitmap_.GetAddressOf());
    if (FAILED(hr)) {
        LOG_ERROR(std::format("CreateBitmapFromDxgiSurface failed: {}",
                              HResultToString(hr)));
        return false;
    }

    d2d_context_->SetTarget(d2d_target_bitmap_.Get());
    return true;
}

bool Direct2DRenderer::UpdateCompositionClip() {
    if (!dcomp_visual_) return true;
    D2D_RECT_F clip = {0.0f, 0.0f, static_cast<float>(width_), static_cast<float>(height_)};
    HRESULT hr = dcomp_visual_->SetClip(clip);
    if (FAILED(hr)) {
        LOG_ERROR(std::format("IDCompositionVisual::SetClip failed: {}",
                              HResultToString(hr)));
        return false;
    }
    return true;
}

bool Direct2DRenderer::CheckDeviceLost() {
    if (!dcomp_device_) return false;
    BOOL lost = FALSE;
    HRESULT hr = dcomp_device_->CheckDeviceState(&lost);
    if (FAILED(hr) || lost) {
        LOG_WARN("DComp device lost detected.");
        return true;
    }
    return false;
}

bool Direct2DRenderer::RecreateDeviceResources() {
    LOG_INFO("Recreating device resources...");
    ReleaseResources();
    return Initialize(hwnd_);
}

void Direct2DRenderer::BeginDraw() {
    if (!d2d_context_) return;

    if (waitable_object_) {
        // 使用超时等待避免渲染线程在退出时无限阻塞
        WaitForSingleObject(waitable_object_, 50);
    }

    d2d_context_->BeginDraw();
}

void Direct2DRenderer::EndDraw() {
    if (!d2d_context_) return;
    HRESULT hr = d2d_context_->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        LOG_WARN("D2D recreate target requested.");
    }
}

void Direct2DRenderer::Present() {
    if (!swap_chain_) return;

    DXGI_PRESENT_PARAMETERS params{};
    RECT dirty_rect = {0, 0, width_, height_};
    params.DirtyRectsCount = 1;
    params.pDirtyRects = &dirty_rect;

    HRESULT hr = swap_chain_->Present1(1, 0, &params);
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
        LOG_WARN(std::format("Device removed/reset during Present: {}",
                             HResultToString(hr)));
    }

    // Ensure DComp re-composites the updated swap chain content.
    if (dcomp_device_) {
        dcomp_device_->Commit();
    }
}

void Direct2DRenderer::Resize(int width, int height) {
    width_ = width;
    height_ = height;

    if (!swap_chain_) return;

    // Atomic size strategy: do not call ResizeBuffers, only update DComp clip and target bitmap
    d2d_target_bitmap_.Reset();
    if (!CreateRenderTargetBitmap()) {
        LOG_ERROR("Failed to recreate render target bitmap after resize.");
    }
    if (!UpdateCompositionClip()) {
        LOG_ERROR("Failed to update composition clip after resize.");
    }
    if (dcomp_device_) {
        dcomp_device_->Commit();
    }
}

void Direct2DRenderer::SetVisible(bool visible) {
    visible_ = visible;
    if (!dcomp_target_ || !dcomp_visual_) return;

    if (visible_) {
        dcomp_target_->SetRoot(dcomp_visual_.Get());
        UpdateCompositionClip();
    } else {
        dcomp_target_->SetRoot(nullptr);
    }
    if (dcomp_device_) {
        dcomp_device_->Commit();
    }
}

// ============================================================================
// IRenderer implementation
// ============================================================================

void Direct2DRenderer::Clear(const Color& color) {
    if (!d2d_context_) return;
    d2d_context_->Clear(ToD2DColor(color));
}

void Direct2DRenderer::PushAxisAlignedClip(const Rect& rect) {
    if (!d2d_context_) return;
    D2D1_RECT_F r = ToD2DRect(rect);
    d2d_context_->PushAxisAlignedClip(&r, D2D1_ANTIALIAS_MODE_ALIASED);
}

void Direct2DRenderer::PopAxisAlignedClip() {
    if (d2d_context_) d2d_context_->PopAxisAlignedClip();
}

void Direct2DRenderer::PushLayer(const Ellipse& clip_ellipse) {
    if (!d2d_context_ || !d2d_factory_ || !d2d_layer_) return;

    D2D1_ELLIPSE d2d_ellipse = ToD2DEllipse(clip_ellipse);

    Microsoft::WRL::ComPtr<ID2D1EllipseGeometry> ellipse_geo;
    HRESULT hr = d2d_factory_->CreateEllipseGeometry(d2d_ellipse, ellipse_geo.GetAddressOf());
    if (FAILED(hr)) return;

    D2D1_LAYER_PARAMETERS1 params = D2D1::LayerParameters1();
    params.geometricMask = ellipse_geo.Get();
    params.maskAntialiasMode = D2D1_ANTIALIAS_MODE_PER_PRIMITIVE;

    d2d_context_->PushLayer(&params, d2d_layer_.Get());
}

void Direct2DRenderer::PopLayer() {
    if (d2d_context_) d2d_context_->PopLayer();
}

void Direct2DRenderer::FillRect(const Rect& rect, const Color& color) {
    if (!d2d_context_) return;
    D2D1_RECT_F r = ToD2DRect(rect);
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;
    d2d_context_->CreateSolidColorBrush(ToD2DColor(color), brush.GetAddressOf());
    d2d_context_->FillRectangle(&r, brush.Get());
}

void Direct2DRenderer::DrawRect(const Rect& rect, const Color& color, float stroke_width) {
    if (!d2d_context_) return;
    D2D1_RECT_F r = ToD2DRect(rect);
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;
    d2d_context_->CreateSolidColorBrush(ToD2DColor(color), brush.GetAddressOf());
    d2d_context_->DrawRectangle(&r, brush.Get(), stroke_width);
}

void Direct2DRenderer::FillRoundedRect(const RoundedRect& rr, const Color& color) {
    if (!d2d_context_) return;
    D2D1_ROUNDED_RECT r = ToD2DRoundedRect(rr);
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;
    d2d_context_->CreateSolidColorBrush(ToD2DColor(color), brush.GetAddressOf());
    d2d_context_->FillRoundedRectangle(r, brush.Get());
}

void Direct2DRenderer::DrawRoundedRect(const RoundedRect& rr, const Color& color, float stroke_width) {
    if (!d2d_context_) return;
    D2D1_ROUNDED_RECT r = ToD2DRoundedRect(rr);
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;
    d2d_context_->CreateSolidColorBrush(ToD2DColor(color), brush.GetAddressOf());
    d2d_context_->DrawRoundedRectangle(r, brush.Get(), stroke_width);
}

void Direct2DRenderer::FillEllipse(const Ellipse& ellipse, const Color& color) {
    if (!d2d_context_) return;
    D2D1_ELLIPSE e = ToD2DEllipse(ellipse);
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;
    d2d_context_->CreateSolidColorBrush(ToD2DColor(color), brush.GetAddressOf());
    d2d_context_->FillEllipse(e, brush.Get());
}

void Direct2DRenderer::DrawEllipse(const Ellipse& ellipse, const Color& color, float stroke_width) {
    if (!d2d_context_) return;
    D2D1_ELLIPSE e = ToD2DEllipse(ellipse);
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;
    d2d_context_->CreateSolidColorBrush(ToD2DColor(color), brush.GetAddressOf());
    d2d_context_->DrawEllipse(e, brush.Get(), stroke_width);
}

void Direct2DRenderer::DrawLine(const Point& p0, const Point& p1, const Color& color, float stroke_width) {
    if (!d2d_context_) return;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;
    d2d_context_->CreateSolidColorBrush(ToD2DColor(color), brush.GetAddressOf());
    d2d_context_->DrawLine(
        D2D1::Point2F(p0.x, p0.y),
        D2D1::Point2F(p1.x, p1.y),
        brush.Get(), stroke_width);
}

TextFormatHandle Direct2DRenderer::CreateTextFormat(const std::wstring& font_family,
                                                      float font_size, bool bold) {
    if (!dwrite_factory_) return 0;

    DWRITE_FONT_WEIGHT weight = bold ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_NORMAL;

    Microsoft::WRL::ComPtr<IDWriteTextFormat> format;
    HRESULT hr = dwrite_factory_->CreateTextFormat(
        font_family.c_str(),
        nullptr,
        weight,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        font_size,
        L"zh-cn",
        format.GetAddressOf());
    if (FAILED(hr)) return 0;

    format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

    for (size_t i = 0; i < text_formats_.size(); ++i) {
        if (!text_formats_[i].format) {
            text_formats_[i].format = format;
            return static_cast<TextFormatHandle>(i + 1);
        }
    }
    text_formats_.push_back({format});
    return static_cast<TextFormatHandle>(text_formats_.size());
}

void Direct2DRenderer::ReleaseTextFormat(TextFormatHandle handle) {
    if (handle == 0 || handle > text_formats_.size()) return;
    text_formats_[handle - 1].format.Reset();
}

IDWriteTextFormat* Direct2DRenderer::GetTextFormat(TextFormatHandle handle) const {
    if (handle == 0 || handle > text_formats_.size()) return nullptr;
    return text_formats_[handle - 1].format.Get();
}

void Direct2DRenderer::DrawString(TextFormatHandle handle, const wchar_t* text, size_t length,
                                const Rect& rect, const Color& color, bool hcenter, bool vcenter) {
    IDWriteTextFormat* format = GetTextFormat(handle);
    if (!format || !d2d_context_ || !dwrite_factory_) return;

    UINT32 len = static_cast<UINT32>(length);
    Microsoft::WRL::ComPtr<IDWriteTextLayout> layout;
    HRESULT hr = dwrite_factory_->CreateTextLayout(
        text, len, format, rect.w, rect.h, layout.GetAddressOf());
    if (FAILED(hr)) return;

    if (hcenter) {
        layout->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    } else {
        layout->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    }

    if (vcenter) {
        layout->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    } else {
        layout->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    }

    DWRITE_TRIMMING trimming{DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0};
    layout->SetTrimming(&trimming, nullptr);

    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;
    d2d_context_->CreateSolidColorBrush(ToD2DColor(color), brush.GetAddressOf());
    d2d_context_->DrawTextLayout(
        D2D1::Point2F(rect.x, rect.y), layout.Get(), brush.Get());
}

BitmapHandle Direct2DRenderer::CreateBitmapFromImageData(const ::overlay::utils::ImageData& image_data) {
    if (!d2d_context_) return 0;

    D2D1_BITMAP_PROPERTIES props{};
    props.pixelFormat.format = DXGI_FORMAT_R8G8B8A8_UNORM;
    props.pixelFormat.alphaMode = D2D1_ALPHA_MODE_STRAIGHT;
    props.dpiX = 96.0f;
    props.dpiY = 96.0f;

    Microsoft::WRL::ComPtr<ID2D1Bitmap> bitmap;
    HRESULT hr = d2d_context_->CreateBitmap(
        D2D1::SizeU(image_data.width, image_data.height),
        image_data.pixels.data(),
        image_data.pitch,
        &props,
        bitmap.GetAddressOf());
    if (FAILED(hr)) {
        LOG_ERROR(std::format("CreateBitmap from image data failed: {}", HResultToString(hr)));
        return 0;
    }

    for (size_t i = 0; i < bitmaps_.size(); ++i) {
        if (!bitmaps_[i].bitmap) {
            bitmaps_[i].bitmap = bitmap;
            return static_cast<BitmapHandle>(i + 1);
        }
    }
    bitmaps_.push_back({bitmap});
    return static_cast<BitmapHandle>(bitmaps_.size());
}

void Direct2DRenderer::ReleaseBitmap(BitmapHandle handle) {
    if (handle == 0 || handle > bitmaps_.size()) return;
    bitmaps_[handle - 1].bitmap.Reset();
}

ID2D1Bitmap* Direct2DRenderer::GetBitmap(BitmapHandle handle) const {
    if (handle == 0 || handle > bitmaps_.size()) return nullptr;
    return bitmaps_[handle - 1].bitmap.Get();
}

void Direct2DRenderer::DrawBitmap(BitmapHandle handle, const Rect& dest_rect, float opacity) {
    ID2D1Bitmap* bitmap = GetBitmap(handle);
    if (!bitmap || !d2d_context_) return;
    D2D1_RECT_F r = ToD2DRect(dest_rect);
    d2d_context_->DrawBitmap(bitmap, &r, opacity,
                             D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, nullptr);
}

void Direct2DRenderer::DrawBitmapHighQuality(
        BitmapHandle handle, const Rect& dest_rect, float opacity) {
    ID2D1Bitmap* bmp = GetBitmap(handle);
    if (!bmp || !d2d_context_) return;

    auto px = bmp->GetPixelSize();
    if (px.width == 0 || px.height == 0) return;

    Microsoft::WRL::ComPtr<ID2D1Effect> effect;
    if (FAILED(d2d_context_->CreateEffect(CLSID_D2D1Scale, &effect))) {
        DrawBitmap(handle, dest_rect, opacity); // fallback
        return;
    }
    effect->SetInput(0, bmp);
    float sx = dest_rect.w / static_cast<float>(px.width);
    float sy = dest_rect.h / static_cast<float>(px.height);
    effect->SetValue(D2D1_SCALE_PROP_SCALE, D2D1_VECTOR_2F{sx, sy});
    effect->SetValue(D2D1_SCALE_PROP_INTERPOLATION_MODE,
                     D2D1_SCALE_INTERPOLATION_MODE_CUBIC);
    effect->SetValue(D2D1_SCALE_PROP_BORDER_MODE, D2D1_BORDER_MODE_HARD);

    D2D1_POINT_2F target_offset = D2D1::Point2F(dest_rect.x, dest_rect.y);
    d2d_context_->DrawImage(effect.Get(),
                            &target_offset,
                            nullptr,
                            D2D1_INTERPOLATION_MODE_LINEAR,
                            D2D1_COMPOSITE_MODE_SOURCE_OVER);
}

} // namespace overlay::overlay
