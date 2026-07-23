#pragma once

#include <windows.h>

namespace overlay::editor {

class EditorWindow {
public:
    EditorWindow();
    ~EditorWindow();

    // 禁止拷贝
    EditorWindow(const EditorWindow&) = delete;
    EditorWindow& operator=(const EditorWindow&) = delete;

    bool Initialize(HWND overlay_hwnd);
    void Shutdown();

    void Show();
    void Hide();
    bool IsOpen() const;

    // 由主循环调用，处理 ImGui 帧；返回 false 表示窗口已请求关闭。
    bool RenderFrame();

private:
    HWND overlay_hwnd_ = nullptr;
    bool initialized_ = false;
    bool open_ = false;
};

} // namespace overlay::editor
