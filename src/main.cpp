#include "core/ConfigManager.h"
#include "core/PlaybackEngine.h"
#include "overlay/Direct2DRenderer.h"
#include "overlay/OverlayWindow.h"
#include "overlay/VisualRenderer.h"
#include "utils/Logger.h"
#include "utils/ResourceLoader.h"
#include "utils/TextEncoding.h"

#include <windows.h>
#include <atomic>
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
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr)) {
        return 1;
    }

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
        CoUninitialize();
        return 1;
    }

    auto active = config_manager.GetActiveRotation();
    if (active) {
        LOG_INFO(std::format("Active rotation: {}", (*active)->name));
    }

    // 初始化图片资源加载器
    overlay::utils::ResourceLoader resource_loader;
    if (!resource_loader.Initialize()) {
        LOG_ERROR("Failed to initialize ResourceLoader.");
        CoUninitialize();
        return 1;
    }

    // 尝试加载背景图和当前角色头像（如缺失则使用回退）
    std::optional<overlay::utils::ImageData> bg_data;
    std::optional<overlay::utils::ImageData> avatar_data;
    const overlay::core::CharacterInfo* current_character = nullptr;
    if (active) {
        if (!(*active)->background_image.empty()) {
            fs::path bg_path = overlay::utils::ResourceLoader::ResolvePath(
                exe_dir, (*active)->background_image);
            bg_data = resource_loader.LoadImage(bg_path);
        }
        current_character = overlay::core::FindCharacter(**active,
            !(*active)->steps.empty() ? (*active)->steps[0].character : "");
        if (current_character && !current_character->avatar_image.empty()) {
            fs::path avatar_path = overlay::utils::ResourceLoader::ResolvePath(
                exe_dir, current_character->avatar_image);
            avatar_data = resource_loader.LoadImage(avatar_path);
        }
    }

    // 创建窗口尺寸
    float window_width = 800.0f;
    float window_height = 120.0f;
    overlay::overlay::Direct2DRenderer renderer;
    overlay::overlay::VisualRenderer visual_renderer(&renderer);
    if (active) {
        visual_renderer.SetRotation(*active);
        float scale = 1.0f;
        try {
            scale = config_manager.GetConfig().settings.at("display").at("scale").get<float>();
        } catch (...) {
            scale = 1.0f;
        }
        visual_renderer.ComputeWindowSize(scale, window_width, window_height);
    }

    // 创建并显示透明 overlay 窗口
    {
        overlay::overlay::OverlayWindow window;
        if (!window.Create(hInstance, L"MingC Key Overlay",
                           100, 100,
                           static_cast<int>(window_width),
                           static_cast<int>(window_height))) {
            LOG_ERROR("Failed to create OverlayWindow.");
            CoUninitialize();
            return 1;
        }
        window.Show();
        LOG_INFO("OverlayWindow shown.");

        HWND hwnd = window.GetHwnd();
        if (!renderer.Initialize(hwnd)) {
            LOG_ERROR("Failed to initialize Direct2DRenderer.");
            CoUninitialize();
            return 1;
        }
        renderer.Resize(static_cast<int>(window_width), static_cast<int>(window_height));

        // 初始化播放引擎（当前用于驱动步骤进度，暂未接入输入检测）
        overlay::core::PlaybackEngine playback_engine;
        if (active) {
            playback_engine.SetRotation(*active);
            playback_engine.Play();
        }

        // 简单渲染循环：自动播放推进并绘制
        std::atomic<bool> running{true};
        std::thread timer([hwnd]() {
            std::this_thread::sleep_for(10s);
            PostMessageW(hwnd, WM_CLOSE, 0, 0);
        });

        std::thread render_thread([&]() {
            using clock = std::chrono::steady_clock;
            auto last_time = clock::now();
            while (running) {
                auto now = clock::now();
                float dt_ms = std::chrono::duration<float, std::milli>(now - last_time).count();
                last_time = now;

                playback_engine.Update(dt_ms);

                overlay::overlay::RenderState state{};
                state.current_step = playback_engine.CurrentStep();
                state.scale = 1.0f;
                try {
                    state.scale = config_manager.GetConfig().settings.at("display").at("scale").get<float>();
                } catch (...) {
                    state.scale = 1.0f;
                }
                state.opacity = 0.85f;
                try {
                    state.opacity = config_manager.GetConfig().settings.at("display").at("opacity").get<float>();
                } catch (...) {
                    state.opacity = 0.85f;
                }
                state.window_width = window_width;
                state.window_height = window_height;
                state.bg_image = bg_data ? &*bg_data : nullptr;
                state.avatar_image = avatar_data ? &*avatar_data : nullptr;

                renderer.BeginDraw();
                renderer.Clear({0.0f, 0.0f, 0.0f, 0.0f});
                visual_renderer.Render(state);
                renderer.EndDraw();
                renderer.Present();

                std::this_thread::sleep_for(std::chrono::milliseconds(16));
            }
        });

        window.RunMessageLoop();
        running = false;
        render_thread.join();
        timer.join();
    }

    LOG_INFO("Application exiting.");
    CoUninitialize();
    return 0;
}
