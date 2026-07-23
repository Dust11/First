#include "editor/EditorWindow.h"

#include "utils/Logger.h"

namespace overlay::editor {

EditorWindow::EditorWindow() = default;

EditorWindow::~EditorWindow() {
    Shutdown();
}

bool EditorWindow::Initialize(HWND overlay_hwnd) {
    overlay_hwnd_ = overlay_hwnd;
    initialized_ = true;
    return true;
}

void EditorWindow::Shutdown() {
    if (!initialized_) return;
    // 任务 14 将在这里释放 ImGui / D3D 资源
    initialized_ = false;
    open_ = false;
}

void EditorWindow::Show() {
    open_ = true;
    // 任务 14 将创建并显示独立编辑器窗口
}

void EditorWindow::Hide() {
    open_ = false;
}

bool EditorWindow::IsOpen() const {
    return open_;
}

bool EditorWindow::RenderFrame() {
    // 任务 14 将渲染 ImGui 帧
    return open_;
}

} // namespace overlay::editor
