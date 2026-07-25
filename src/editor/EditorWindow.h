#pragma once

#include "core/ConfigManager.h"
#include "editor/EditorComponents.h"

#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <windows.h>

#include <functional>
#include <atomic>
#include <mutex>

namespace overlay::editor {

class EditorWindow {
public:
    EditorWindow();
    ~EditorWindow();

    EditorWindow(const EditorWindow&) = delete;
    EditorWindow& operator=(const EditorWindow&) = delete;

    // overlay_hwnd: 用于决定编辑器首次显示位置。
    // device: 与 overlay D2D 渲染器共享的 D3D11 设备。
    bool Initialize(HWND overlay_hwnd, ID3D11Device* device);
    void Shutdown();

    void SetConfigManager(overlay::core::ConfigManager* config_manager);
    void SetApplyCallback(std::function<void()> callback);

    void Show();
    void Hide();
    bool IsOpen() const;

    // 由渲染线程调用，处理并渲染一帧 ImGui；返回 false 表示窗口已关闭。
    bool RenderFrame();

private:
    bool RegisterWindowClass();
    bool CreateEditorWindow();
    bool CreateSwapChain();
    bool LoadFonts();
    void ResizeSwapChain(int width, int height);

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    HWND overlay_hwnd_ = nullptr;
    HWND hwnd_ = nullptr;
    HINSTANCE hinstance_ = nullptr;
    wchar_t class_name_[64] = L"MingCEditorWindow";

    ID3D11Device* d3d_device_ = nullptr;
    ID3D11DeviceContext* d3d_context_ = nullptr;
    IDXGISwapChain1* swap_chain_ = nullptr;
    ID3D11RenderTargetView* rtv_ = nullptr;

    bool initialized_ = false;
    // open_ 由主线程（Show/Hide/WM_CLOSE）写入、渲染线程（RenderFrame）读取
    std::atomic<bool> open_{false};
    bool imgui_backends_initialized_ = false;
    int width_ = 1000;
    int height_ = 700;

    // 主线程 WM_SIZE 只记录目标尺寸，渲染线程在 RenderFrame 中应用，
    // 避免与渲染线程并发访问 rtv_ / d3d_context_
    std::mutex resize_mutex_;
    int pending_resize_w_ = 0;
    int pending_resize_h_ = 0;

    overlay::core::ConfigManager* config_manager_ = nullptr;
    std::function<void()> apply_callback_;
    EditorComponents components_;
};

} // namespace overlay::editor
