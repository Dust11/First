#pragma once

#include <windows.h>

#include <functional>
#include <string>

namespace overlay::overlay {

class IRenderer;

class OverlayWindow {
public:
    OverlayWindow();
    ~OverlayWindow();

    // 禁止拷贝
    OverlayWindow(const OverlayWindow&) = delete;
    OverlayWindow& operator=(const OverlayWindow&) = delete;

    bool Create(HINSTANCE hInstance, const std::wstring& title, int x, int y,
                int width, int height);
    void Destroy();

    void Show();
    void Hide();
    bool IsVisible() const { return visible_; }

    void SetPosition(int x, int y);
    void SetSize(int width, int height);
    void SetScale(float scale);

    void SetMoveMode(bool enabled);
    bool IsMoveMode() const { return move_mode_; }

    void RunMessageLoop();
    void PostQuit();

    HWND GetHwnd() const { return hwnd_; }
    int GetClientWidth() const;
    int GetClientHeight() const;

    // 按键/窗口事件回调，由 PlaybackEngine/KeyDetector 注册
    using KeyEventCallback = std::function<void(UINT msg, WPARAM wParam, LPARAM lParam)>;
    void SetKeyEventCallback(KeyEventCallback cb) { key_event_callback_ = std::move(cb); }

    // WM_HOTKEY 消息回调
    using HotkeyMessageCallback = std::function<void(WPARAM wParam)>;
    void SetHotkeyMessageCallback(HotkeyMessageCallback cb) { hotkey_callback_ = std::move(cb); }

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    bool RegisterWindowClass(HINSTANCE hInstance);

    HINSTANCE hinstance_ = nullptr;
    HWND hwnd_ = nullptr;
    std::wstring class_name_;
    std::wstring title_;

    int x_ = 100;
    int y_ = 100;
    int width_ = 800;
    int height_ = 120;
    float scale_ = 1.0f;

    bool visible_ = false;
    bool move_mode_ = false;
    bool dragging_ = false;
    POINT drag_start_pos_{};
    POINT drag_start_window_pos_{};

    KeyEventCallback key_event_callback_;
    HotkeyMessageCallback hotkey_callback_;
};

} // namespace overlay::overlay
