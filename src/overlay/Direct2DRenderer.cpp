#include "overlay/Direct2DRenderer.h"

#include "utils/Logger.h"

#include <algorithm>
#include <format>

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

    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, options,
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
        WaitForSingleObject(waitable_object_, INFINITE);
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
}

void Direct2DRenderer::Resize(int width, int height) {
    width_ = width;
    height_ = height;

    if (!swap_chain_) return;

    // 原子大小策略：不调用 ResizeBuffers，只更新 DComp clip 和目标位图
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

} // namespace overlay::overlay
