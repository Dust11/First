#pragma once

#include <windows.h>

#include "overlay/IRenderer.h"

namespace overlay::overlay {

class Direct2DRenderer : public IRenderer {
public:
    Direct2DRenderer() = default;
    bool Initialize(HWND hwnd) { return true; }
    void Render() {}
    void Resize(int width, int height) {}
    void Present() {}
};

} // namespace overlay::overlay
