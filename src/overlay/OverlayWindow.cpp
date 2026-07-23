#include "overlay/OverlayWindow.h"

#include "utils/Logger.h"

#include <algorithm>
#include <windowsx.h>

#include <format>

namespace overlay::overlay {

namespace {

constexpr wchar_t kClassName[] = L"MingCKeyOverlayWindow";

} // namespace

OverlayWindow::OverlayWindow() = default;

OverlayWindow::~OverlayWindow() {
    Destroy();
}

bool OverlayWindow::RegisterWindowClass(HINSTANCE hInstance) {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = &OverlayWindow::WindowProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW));
    wc.hbrBackground = nullptr;
    wc.lpszClassName = kClassName;

    if (!RegisterClassExW(&wc)) {
        LOG_ERROR(std::format("RegisterClassExW failed, error={}", GetLastError()));
        return false;
    }
    return true;
}

bool OverlayWindow::Create(HINSTANCE hInstance, const std::wstring& title, int x, int y,
                           int width, int height) {
    if (hwnd_) return true;

    hinstance_ = hInstance;
    title_ = title;
    x_ = x;
    y_ = y;
    width_ = width;
    height_ = height;

    if (!RegisterWindowClass(hInstance)) return false;

    DWORD ex_style = WS_EX_NOREDIRECTIONBITMAP | WS_EX_TRANSPARENT |
                     WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE;
    DWORD style = WS_POPUP;

    hwnd_ = CreateWindowExW(
        ex_style, kClassName, title_.c_str(), style,
        x_, y_, width_, height_,
        nullptr, nullptr, hinstance_, this);

    if (!hwnd_) {
        LOG_ERROR(std::format("CreateWindowExW failed, error={}", GetLastError()));
        return false;
    }

    LOG_INFO("OverlayWindow created.");
    return true;
}

void OverlayWindow::Destroy() {
    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
    if (hinstance_) {
        UnregisterClassW(kClassName, hinstance_);
        hinstance_ = nullptr;
    }
}

void OverlayWindow::Show() {
    if (!hwnd_) return;
    ShowWindow(hwnd_, SW_SHOWNA);
    visible_ = true;
}

void OverlayWindow::Hide() {
    if (!hwnd_) return;
    ShowWindow(hwnd_, SW_HIDE);
    visible_ = false;
}

void OverlayWindow::SetPosition(int x, int y) {
    x_ = x;
    y_ = y;
    if (hwnd_) {
        SetWindowPos(hwnd_, nullptr, x_, y_, 0, 0,
                     SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
    }
}

void OverlayWindow::SetSize(int width, int height) {
    width_ = width;
    height_ = height;
    if (hwnd_) {
        SetWindowPos(hwnd_, nullptr, 0, 0, width_, height_,
                     SWP_NOZORDER | SWP_NOMOVE | SWP_NOACTIVATE);
    }
}

void OverlayWindow::SetScale(float scale) {
    scale_ = scale;
    // 缩放锚定于窗口左上角；后续任务支持鼠标锚点
    int new_w = static_cast<int>(width_ * scale_);
    int new_h = static_cast<int>(height_ * scale_);
    SetSize(new_w, new_h);
}

void OverlayWindow::SetMoveMode(bool enabled) {
    if (move_mode_ == enabled) return;
    move_mode_ = enabled;
    if (!hwnd_) return;

    LONG ex_style = GetWindowLongW(hwnd_, GWL_EXSTYLE);
    if (move_mode_) {
        // 移动模式下临时移除点击穿透
        ex_style &= ~WS_EX_TRANSPARENT;
        SetWindowLongW(hwnd_, GWL_EXSTYLE, ex_style);
        SetWindowPos(hwnd_, nullptr, 0, 0, 0, 0,
                     SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE |
                         SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    } else {
        ex_style |= WS_EX_TRANSPARENT;
        SetWindowLongW(hwnd_, GWL_EXSTYLE, ex_style);
        SetWindowPos(hwnd_, nullptr, 0, 0, 0, 0,
                     SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE |
                         SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }
    LOG_INFO(std::format("Move mode {}", move_mode_ ? "enabled" : "disabled"));
}

void OverlayWindow::RunMessageLoop() {
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

void OverlayWindow::PostQuit() {
    PostQuitMessage(0);
}

LRESULT CALLBACK OverlayWindow::WindowProc(HWND hwnd, UINT msg, WPARAM wParam,
                                           LPARAM lParam) {
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<LPCREATESTRUCT>(lParam);
        auto* window = static_cast<OverlayWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
        return TRUE;
    }

    auto* window = reinterpret_cast<OverlayWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (window) {
        return window->HandleMessage(msg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT OverlayWindow::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_DPICHANGED: {
        // 高 DPI 变更：使用系统建议矩形
        auto* const rect = reinterpret_cast<RECT*>(lParam);
        SetWindowPos(hwnd_, nullptr, rect->left, rect->top,
                     rect->right - rect->left,
                     rect->bottom - rect->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    }

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE && move_mode_) {
            SetMoveMode(false);
            return 0;
        }
        if (key_event_callback_) {
            key_event_callback_(msg, wParam, lParam);
        }
        return 0;

    case WM_KEYUP:
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
        if (key_event_callback_) {
            key_event_callback_(msg, wParam, lParam);
        }
        return 0;

    case WM_LBUTTONDOWN:
        if (move_mode_) {
            dragging_ = true;
            drag_start_pos_.x = GET_X_LPARAM(lParam);
            drag_start_pos_.y = GET_Y_LPARAM(lParam);
            RECT rc{};
            GetWindowRect(hwnd_, &rc);
            drag_start_window_pos_.x = rc.left;
            drag_start_window_pos_.y = rc.top;
            SetCapture(hwnd_);
        }
        return 0;

    case WM_MOUSEMOVE:
        if (move_mode_ && dragging_) {
            int dx = GET_X_LPARAM(lParam) - drag_start_pos_.x;
            int dy = GET_Y_LPARAM(lParam) - drag_start_pos_.y;
            SetPosition(drag_start_window_pos_.x + dx, drag_start_window_pos_.y + dy);
        }
        return 0;

    case WM_LBUTTONUP:
        if (move_mode_) {
            dragging_ = false;
            ReleaseCapture();
        }
        return 0;

    case WM_MOUSEWHEEL:
        if (move_mode_) {
            int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            float new_scale = scale_ + (delta > 0 ? 0.1f : -0.1f);
            new_scale = std::max(0.5f, std::min(3.0f, new_scale));
            SetScale(new_scale);
        }
        return 0;

    default:
        return DefWindowProcW(hwnd_, msg, wParam, lParam);
    }
}

} // namespace overlay::overlay
