#include "editor/EditorWindow.h"

#include "utils/Logger.h"
#include "utils/TextEncoding.h"

#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

// imgui_impl_win32.h 有意将 WndProcHandler 放在 #if 0 中，需自行前向声明
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

#include <algorithm>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

namespace overlay::editor {

namespace {

constexpr int kInitialWidth = 1000;
constexpr int kInitialHeight = 700;
// app.rc 中的图标资源 ID
constexpr int kAppIconId = 101;

bool FontFileExists(const wchar_t* path) {
    DWORD attr = GetFileAttributesW(path);
    return attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

} // namespace

EditorWindow::EditorWindow() = default;

EditorWindow::~EditorWindow() {
    Shutdown();
}

bool EditorWindow::RegisterWindowClass() {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = &EditorWindow::WindowProc;
    wc.hInstance = hinstance_;
    wc.hCursor = LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW));
    wc.hbrBackground = nullptr;
    wc.lpszClassName = class_name_;
    // 标题栏/任务栏图标
    wc.hIcon = LoadIconW(hinstance_, MAKEINTRESOURCEW(kAppIconId));
    wc.hIconSm = static_cast<HICON>(LoadImageW(
        hinstance_, MAKEINTRESOURCEW(kAppIconId), IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0));

    if (!RegisterClassExW(&wc)) {
        LOG_ERROR(std::format("Editor RegisterClassExW failed, error={}", GetLastError()));
        return false;
    }
    return true;
}

bool EditorWindow::CreateEditorWindow() {
    int x = CW_USEDEFAULT;
    int y = CW_USEDEFAULT;
    if (overlay_hwnd_ != nullptr && IsWindow(overlay_hwnd_)) {
        RECT rc{};
        GetWindowRect(overlay_hwnd_, &rc);
        x = rc.left + 40;
        y = rc.top - kInitialHeight - 20;
        if (y < 0) y = rc.bottom + 20;
    }

    hwnd_ = CreateWindowExW(
        WS_EX_OVERLAPPEDWINDOW,
        class_name_,
        L"MingC Key Overlay - 编辑器",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        x, y, kInitialWidth, kInitialHeight,
        nullptr, nullptr, hinstance_, this);

    if (!hwnd_) {
        LOG_ERROR(std::format("Editor CreateWindowExW failed, error={}", GetLastError()));
        return false;
    }

    width_ = kInitialWidth;
    height_ = kInitialHeight;
    return true;
}

bool EditorWindow::CreateSwapChain() {
    if (!d3d_device_) return false;

    Microsoft::WRL::ComPtr<IDXGIDevice> dxgi_device;
    HRESULT hr = d3d_device_->QueryInterface(IID_PPV_ARGS(&dxgi_device));
    if (FAILED(hr)) {
        LOG_ERROR(std::format("Editor QueryInterface IDXGIDevice failed: 0x{:08X}", static_cast<uint32_t>(hr)));
        return false;
    }

    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
    hr = dxgi_device->GetAdapter(adapter.GetAddressOf());
    if (FAILED(hr)) {
        LOG_ERROR(std::format("Editor GetAdapter failed: 0x{:08X}", static_cast<uint32_t>(hr)));
        return false;
    }

    Microsoft::WRL::ComPtr<IDXGIFactory2> factory;
    hr = adapter->GetParent(IID_PPV_ARGS(&factory));
    if (FAILED(hr)) {
        LOG_ERROR(std::format("Editor GetParent IDXGIFactory2 failed: 0x{:08X}", static_cast<uint32_t>(hr)));
        return false;
    }

    DXGI_SWAP_CHAIN_DESC1 sd{};
    sd.Width = width_;
    sd.Height = height_;
    sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferCount = 2;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    sd.Flags = 0;

    hr = factory->CreateSwapChainForHwnd(
        d3d_device_, hwnd_, &sd, nullptr, nullptr, &swap_chain_);
    if (FAILED(hr)) {
        LOG_ERROR(std::format("Editor CreateSwapChainForHwnd failed: 0x{:08X}", static_cast<uint32_t>(hr)));
        return false;
    }

    // 禁用 Alt+Enter 全屏切换
    factory->MakeWindowAssociation(hwnd_, DXGI_MWA_NO_ALT_ENTER);

    ResizeSwapChain(width_, height_);
    return true;
}

void EditorWindow::ResizeSwapChain(int width, int height) {
    if (!swap_chain_ || !d3d_context_) return;

    if (rtv_) {
        rtv_->Release();
        rtv_ = nullptr;
    }

    HRESULT hr = swap_chain_->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(hr)) {
        LOG_ERROR(std::format("Editor ResizeBuffers failed: 0x{:08X}", static_cast<uint32_t>(hr)));
        return;
    }

    width_ = width;
    height_ = height;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> back_buffer;
    hr = swap_chain_->GetBuffer(0, IID_PPV_ARGS(&back_buffer));
    if (FAILED(hr)) {
        LOG_ERROR(std::format("Editor GetBuffer failed: 0x{:08X}", static_cast<uint32_t>(hr)));
        return;
    }

    hr = d3d_device_->CreateRenderTargetView(back_buffer.Get(), nullptr, &rtv_);
    if (FAILED(hr)) {
        LOG_ERROR(std::format("Editor CreateRenderTargetView failed: 0x{:08X}", static_cast<uint32_t>(hr)));
    }
}

bool EditorWindow::LoadFonts() {
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();

    ImFontConfig config;
    config.MergeMode = false;

    // 尝试加载系统微软雅黑；失败则回退默认字体
    const wchar_t* kFontPaths[] = {
        L"C:\\Windows\\Fonts\\msyh.ttc",
        L"C:\\Windows\\Fonts\\msyhbd.ttc",
        L"C:\\Windows\\Fonts\\simhei.ttf",
    };
    ImFont* font = nullptr;
    for (const wchar_t* path : kFontPaths) {
        if (FontFileExists(path)) {
            std::string utf8_path = ::overlay::utils::WstringToUtf8(path);
            font = io.Fonts->AddFontFromFileTTF(
                utf8_path.c_str(), 18.0f, &config,
                io.Fonts->GetGlyphRangesChineseFull());
            if (font) break;
        }
    }
    if (!font) {
        LOG_WARN("Failed to load Chinese system font; using ImGui default font.");
        io.Fonts->AddFontDefault();
    } else {
        LOG_INFO("Editor Chinese font loaded.");
    }
    return true;
}

bool EditorWindow::Initialize(HWND overlay_hwnd, ID3D11Device* device) {
    if (initialized_) return true;
    overlay_hwnd_ = overlay_hwnd;
    d3d_device_ = device;
    if (!d3d_device_) {
        LOG_ERROR("EditorWindow::Initialize received null D3D11 device.");
        return false;
    }
    d3d_device_->GetImmediateContext(&d3d_context_);

    hinstance_ = GetModuleHandleW(nullptr);
    if (!RegisterWindowClass()) return false;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    LoadFonts();

    initialized_ = true;
    return true;
}

void EditorWindow::Shutdown() {
    if (!initialized_) return;

    components_.OnEditorHidden();

    if (imgui_backends_initialized_) {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        imgui_backends_initialized_ = false;
    }
    ImGui::DestroyContext();

    if (rtv_) {
        rtv_->Release();
        rtv_ = nullptr;
    }
    if (swap_chain_) {
        swap_chain_->Release();
        swap_chain_ = nullptr;
    }
    if (d3d_context_) {
        d3d_context_->Release();
        d3d_context_ = nullptr;
    }
    d3d_device_ = nullptr;

    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
    if (hinstance_) {
        UnregisterClassW(class_name_, hinstance_);
    }

    initialized_ = false;
    open_.store(false, std::memory_order_release);
}

void EditorWindow::SetConfigManager(overlay::core::ConfigManager* config_manager) {
    config_manager_ = config_manager;
}

void EditorWindow::SetApplyCallback(std::function<void()> callback) {
    apply_callback_ = std::move(callback);
}

void EditorWindow::Show() {
    if (!initialized_) return;
    if (open_.load(std::memory_order_acquire)) return;

    // 窗口必须在主线程创建（Show 由主线程热键消息触发）：
    // 窗口消息按线程亲和性投递，若窗口在渲染线程创建，其消息会进入
    // 渲染线程队列，而渲染线程没有消息循环，导致编辑器“未响应”。
    if (!hwnd_) {
        if (!CreateEditorWindow()) return;
        ImGui_ImplWin32_Init(hwnd_);
        ImGui_ImplDX11_Init(d3d_device_, d3d_context_);
        imgui_backends_initialized_ = true;
        CreateSwapChain();
    }

    ShowWindow(hwnd_, SW_SHOW);
    SetForegroundWindow(hwnd_);
    open_.store(true, std::memory_order_release);
}

void EditorWindow::Hide() {
    open_.store(false, std::memory_order_release);
    components_.OnEditorHidden();
    if (hwnd_) {
        ShowWindow(hwnd_, SW_HIDE);
    }
}

bool EditorWindow::IsOpen() const {
    return open_.load(std::memory_order_acquire);
}

bool EditorWindow::RenderFrame() {
    if (!initialized_ || !open_.load(std::memory_order_acquire)) return false;
    if (!hwnd_ || !swap_chain_ || !rtv_) return open_.load(std::memory_order_acquire);

    // 处理窗口关闭（WM_CLOSE 会设置 open_ = false）
    if (!IsWindow(hwnd_)) {
        open_.store(false, std::memory_order_release);
        return false;
    }

    // 应用主线程挂起的窗口尺寸变更（ResizeSwapChain 涉及 rtv_，
    // 必须与下方渲染在同一线程执行）
    int pending_w = 0;
    int pending_h = 0;
    {
        std::lock_guard<std::mutex> lock(resize_mutex_);
        pending_w = pending_resize_w_;
        pending_h = pending_resize_h_;
        pending_resize_w_ = 0;
        pending_resize_h_ = 0;
    }
    if (pending_w > 0 && pending_h > 0 && (pending_w != width_ || pending_h != height_)) {
        ResizeSwapChain(pending_w, pending_h);
    }

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // 任务 15：编辑器 UI
    ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());
    components_.Draw(config_manager_, d3d_device_, apply_callback_);

    ImGui::Render();

    const float clear_color[4] = {0.1f, 0.1f, 0.13f, 1.0f};
    d3d_context_->OMSetRenderTargets(1, &rtv_, nullptr);
    d3d_context_->ClearRenderTargetView(rtv_, clear_color);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    swap_chain_->Present(1, 0);
    return open_.load(std::memory_order_acquire);
}

LRESULT CALLBACK EditorWindow::WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<LPCREATESTRUCT>(lParam);
        auto* window = static_cast<EditorWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
        return TRUE;
    }

    auto* window = reinterpret_cast<EditorWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (window) {
        return window->HandleMessage(msg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT EditorWindow::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd_, msg, wParam, lParam)) {
        return TRUE;
    }

    switch (msg) {
    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED) {
            // 只记录尺寸，实际 ResizeBuffers 由渲染线程在 RenderFrame 中执行
            std::lock_guard<std::mutex> lock(resize_mutex_);
            pending_resize_w_ = static_cast<int>(LOWORD(lParam));
            pending_resize_h_ = static_cast<int>(HIWORD(lParam));
        }
        return 0;

    case WM_CLOSE:
        open_.store(false, std::memory_order_release);
        ShowWindow(hwnd_, SW_HIDE);
        return 0;

    case WM_DESTROY:
        hwnd_ = nullptr;
        return 0;

    default:
        return DefWindowProcW(hwnd_, msg, wParam, lParam);
    }
}

} // namespace overlay::editor
