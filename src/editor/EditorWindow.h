#pragma once

namespace overlay::editor {

class EditorWindow {
public:
    EditorWindow() = default;
    bool Initialize() { return true; }
    void Show() {}
    void Hide() {}
    void Render() {}
    void Shutdown() {}
};

} // namespace overlay::editor
