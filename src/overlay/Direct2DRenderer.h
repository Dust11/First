#pragma once

#include "overlay/IRenderer.h"

#include <d2d1_2.h>
#include <dcomp.h>
#include <dxgi1_2.h>
#include <d3d11_1.h>
#include <wrl/client.h>

#include <windows.h>

namespace overlay::overlay {

class Direct2DRenderer : public IRenderer {
public:
    Direct2DRenderer();
    ~Direct2DRenderer() override;

    Direct2DRenderer(const Direct2DRenderer&) = delete;
    Direct2DRenderer& operator=(const Direct2DRenderer&) = delete;

    bool Initialize(HWND hwnd);
    void Shutdown();

    // 设备丢失检测与恢复
    bool CheckDeviceLost();
    bool RecreateDeviceResources();

    // 渲染帧
    void BeginDraw();
    void EndDraw();
    void Present();

    void Resize(int width, int height);
    void SetVisible(bool visible);

    ID2D1DeviceContext* GetContext() const { return d2d_context_.Get(); }
    ID3D11Device* GetD3DDevice() const { return d3d_device_.Get(); }

private:
    bool CreateD3DDevice();
    bool CreateSwapChain();
    bool CreateD2DResources();
    bool CreateDCompResources();
    bool CreateRenderTargetBitmap();
    void ReleaseResources();

    bool UpdateCompositionClip();

    HWND hwnd_ = nullptr;
    int width_ = 0;
    int height_ = 0;
    bool visible_ = true;

    // 原子大小交换链：创建时取显示器最大分辨率，后续用 clip 控制
    int max_width_ = 0;
    int max_height_ = 0;

    Microsoft::WRL::ComPtr<ID3D11Device> d3d_device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3d_context_;
    Microsoft::WRL::ComPtr<IDXGIDevice1> dxgi_device_;
    Microsoft::WRL::ComPtr<IDXGIAdapter> dxgi_adapter_;
    Microsoft::WRL::ComPtr<IDXGIFactory2> dxgi_factory_;

    Microsoft::WRL::ComPtr<IDXGISwapChain2> swap_chain_;
    HANDLE waitable_object_ = nullptr;

    Microsoft::WRL::ComPtr<ID2D1Factory2> d2d_factory_;
    Microsoft::WRL::ComPtr<ID2D1Device1> d2d_device_;
    Microsoft::WRL::ComPtr<ID2D1DeviceContext> d2d_context_;
    Microsoft::WRL::ComPtr<ID2D1Bitmap1> d2d_target_bitmap_;

    Microsoft::WRL::ComPtr<IDCompositionDevice> dcomp_device_;
    Microsoft::WRL::ComPtr<IDCompositionTarget> dcomp_target_;
    Microsoft::WRL::ComPtr<IDCompositionVisual> dcomp_visual_;
};

} // namespace overlay::overlay
