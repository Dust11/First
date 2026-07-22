#include "core/ConfigManager.h"
#include "overlay/Direct2DRenderer.h"
#include "overlay/OverlayWindow.h"
#include "utils/Logger.h"
#include "utils/TextEncoding.h"

#include <windows.h>
#include <chrono>
#include <filesystem>
#include <format>
#include <thread>

namespace fs = std::filesystem;
using namespace std::chrono_literals;

static fs::path GetExeDirectory() {
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    return fs::path(path).parent_path();
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int) {
    fs::path exe_dir = GetExeDirectory();
    overlay::utils::Logger::Instance().Init(exe_dir / "overlay.log");
    LOG_INFO("Application started.");

    // 验证 TextEncoding 中文字符串往返
    {
        const std::string utf8 = "中文字符测试 · 散华";
        const std::wstring wstr = overlay::utils::Utf8ToWstring(utf8);
        const std::string back = overlay::utils::WstringToUtf8(wstr);
        if (back == utf8) {
            LOG_INFO("TextEncoding round-trip OK.");
        } else {
            LOG_ERROR("TextEncoding round-trip failed.");
        }
    }

    overlay::core::ConfigManager config_manager(exe_dir);
    if (!config_manager.Load()) {
        LOG_ERROR("Failed to load config.");
        return 1;
    }

    auto active = config_manager.GetActiveRotation();
    if (active) {
        LOG_INFO(std::format("Active rotation: {}", (*active)->name));
    }

    // 创建并显示透明 overlay 窗口（2 秒后自动退出，用于验证骨架）
    {
        overlay::overlay::OverlayWindow window;
        if (!window.Create(hInstance, L"MingC Key Overlay", 100, 100, 800, 120)) {
            LOG_ERROR("Failed to create OverlayWindow.");
            return 1;
        }
        window.Show();
        LOG_INFO("OverlayWindow shown.");

        // 初始化 Direct2D + DirectComposition 渲染器并绘制一帧透明背景
        overlay::overlay::Direct2DRenderer renderer;
        HWND hwnd = window.GetHwnd();
        if (!renderer.Initialize(hwnd)) {
            LOG_ERROR("Failed to initialize Direct2DRenderer.");
            return 1;
        }
        renderer.Resize(800, 120);
        renderer.BeginDraw();
        if (auto* ctx = renderer.GetContext()) {
            ctx->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));
        }
        renderer.EndDraw();
        renderer.Present();
        LOG_INFO("Direct2DRenderer presented one frame.");

        std::thread timer([hwnd]() {
            std::this_thread::sleep_for(2s);
            PostMessageW(hwnd, WM_CLOSE, 0, 0);
        });
        window.RunMessageLoop();
        timer.join();
    }

    LOG_INFO("Application exiting.");
    return 0;
}
