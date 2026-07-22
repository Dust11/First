#pragma once

namespace overlay::overlay {

class OverlayWindow {
public:
    OverlayWindow() = default;
    bool Create() { return true; }
    void Show() {}
    void Hide() {}
    void Run() {}
    void Destroy() {}
};

} // namespace overlay::overlay
