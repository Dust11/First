#include "core/ConfigManager.h"
#include "core/KeyDetector.h"
#include "core/PlaybackEngine.h"
#include "editor/EditorWindow.h"
#include "overlay/Direct2DRenderer.h"
#include "overlay/OverlayWindow.h"
#include "overlay/VisualRenderer.h"
#include "utils/FileWatcher.h"
#include "utils/HotkeyManager.h"
#include "utils/Logger.h"
#include "utils/ResourceLoader.h"
#include "utils/TextEncoding.h"

#include <windows.h>
#include <commctrl.h>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <format>
#include <memory>
#include <mutex>
#include <thread>

namespace fs = std::filesystem;
using namespace std::chrono_literals;

static fs::path GetExeDirectory() {
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    return fs::path(path).parent_path();
}

// 应用级共享状态
struct AppContext {
    HINSTANCE hInstance = nullptr;
    fs::path exe_dir;

    overlay::core::ConfigManager* config_manager = nullptr;
    overlay::core::PlaybackEngine* playback_engine = nullptr;
    overlay::utils::HotkeyManager* hotkey_manager = nullptr;
    overlay::utils::ResourceLoader* resource_loader = nullptr;
    overlay::overlay::OverlayWindow* window = nullptr;
    overlay::overlay::Direct2DRenderer* renderer = nullptr;
    overlay::overlay::VisualRenderer* visual_renderer = nullptr;
    overlay::editor::EditorWindow* editor = nullptr;
    overlay::utils::FileWatcher* file_watcher = nullptr;

    overlay::core::EventDrivenKeyDetector event_detector;
    overlay::core::PollingKeyDetector polling_detector;
    overlay::core::IKeyDetector* active_detector = nullptr;

    std::atomic<bool> running{true};
    std::atomic<bool> visible{true};
    std::atomic<bool> wrong_key_flash{false};
    std::chrono::steady_clock::time_point wrong_flash_start;

    std::mutex config_mutex;

    std::optional<overlay::utils::ImageData> bg_data;
    std::optional<overlay::utils::ImageData> avatar_data;
    size_t last_avatar_step = std::numeric_limits<size_t>::max();

    std::string mode = "auto";
};

static std::string GetModeString(const overlay::core::ConfigManager& cm) {
    try {
        return cm.GetConfig().settings.at("display").at("mode").get<std::string>();
    } catch (...) {
        return "auto";
    }
}

static bool ModeIsKey(const std::string& mode) {
    return mode == "key" || mode == "input";
}

static float GetScale(const overlay::core::ConfigManager& cm) {
    try {
        return cm.GetConfig().settings.at("display").at("scale").get<float>();
    } catch (...) {
        return 1.0f;
    }
}

static float GetOpacity(const overlay::core::ConfigManager& cm) {
    try {
        return cm.GetConfig().settings.at("display").at("opacity").get<float>();
    } catch (...) {
        return 0.85f;
    }
}

static bool GetBoolSetting(const overlay::core::ConfigManager& cm,
                           const std::string& section, const std::string& key,
                           bool default_value) {
    try {
        return cm.GetConfig().settings.at(section).at(key).get<bool>();
    } catch (...) {
        return default_value;
    }
}

static int GetIntSetting(const overlay::core::ConfigManager& cm,
                         const std::string& section, const std::string& key,
                         int default_value) {
    try {
        return cm.GetConfig().settings.at(section).at(key).get<int>();
    } catch (...) {
        return default_value;
    }
}

static const overlay::core::TeamRotation* GetActiveRotation(
    const overlay::core::ConfigManager& cm) {
    auto opt = const_cast<overlay::core::ConfigManager&>(cm).GetActiveRotation();
    if (opt) return *opt;
    return nullptr;
}

static void LoadCurrentImages(AppContext& ctx) {
    if (!ctx.config_manager || !ctx.resource_loader) return;

    const auto* rotation = GetActiveRotation(*ctx.config_manager);
    if (!rotation) return;

    // 背景图只加载一次（流程切换时重载）
    if (!rotation->background_image.empty() && !ctx.bg_data) {
        fs::path bg_path = overlay::utils::ResourceLoader::ResolvePath(
            ctx.exe_dir, rotation->background_image);
        ctx.bg_data = ctx.resource_loader->LoadImage(bg_path);
    }

    size_t step = ctx.playback_engine ? ctx.playback_engine->CurrentStep() : 0;
    if (step == ctx.last_avatar_step && ctx.avatar_data) return;
    ctx.last_avatar_step = step;

    const auto* character = overlay::core::FindCharacter(
        *rotation, step < rotation->steps.size() ? rotation->steps[step].character : "");
    if (character && !character->avatar_image.empty()) {
        fs::path avatar_path = overlay::utils::ResourceLoader::ResolvePath(
            ctx.exe_dir, character->avatar_image);
        ctx.avatar_data = ctx.resource_loader->LoadImage(avatar_path);
    } else {
        ctx.avatar_data = std::nullopt;
    }
}

static void ApplyActiveRotation(AppContext& ctx) {
    if (!ctx.config_manager || !ctx.playback_engine || !ctx.visual_renderer) return;

    const auto* rotation = GetActiveRotation(*ctx.config_manager);
    if (!rotation) return;

    ctx.playback_engine->SetRotation(rotation);
    ctx.playback_engine->Play();
    ctx.visual_renderer->SetRotation(rotation);
    ctx.last_avatar_step = std::numeric_limits<size_t>::max();
    ctx.bg_data = std::nullopt;
    ctx.avatar_data = std::nullopt;
    LoadCurrentImages(ctx);

    // 更新按键检测器的有效键集合
    std::unordered_set<std::string> valid_keys = overlay::core::ExtractValidKeys(*rotation);
    ctx.event_detector.SetValidKeys(valid_keys);
    ctx.polling_detector.SetValidKeys(valid_keys);
}

static void SwitchKeyDetector(AppContext& ctx) {
    if (ModeIsKey(ctx.mode)) {
        if (ctx.active_detector != &ctx.polling_detector) {
            int poll_hz = std::clamp(GetIntSetting(*ctx.config_manager, "input", "poll_hz", 60),
                                     30, 120);
            ctx.polling_detector.Start(poll_hz);
            ctx.active_detector = &ctx.polling_detector;
            LOG_INFO("Switched to polling key detector.");
        }
    } else {
        if (ctx.active_detector == &ctx.polling_detector) {
            ctx.polling_detector.Stop();
        }
        ctx.active_detector = &ctx.event_detector;
        LOG_INFO("Switched to event-driven key detector (auto mode).");
    }
}

static void ToggleMode(AppContext& ctx) {
    if (!ctx.config_manager) return;
    ctx.mode = ModeIsKey(ctx.mode) ? "auto" : "key";
    try {
        ctx.config_manager->GetConfig().settings["display"]["mode"] = ctx.mode;
    } catch (...) {}
    SwitchKeyDetector(ctx);
}

static void NextRotation(AppContext& ctx) {
    if (!ctx.config_manager || ctx.config_manager->GetConfig().rotations.empty()) return;
    const auto& rotations = ctx.config_manager->GetConfig().rotations;
    size_t idx = 0;
    for (size_t i = 0; i < rotations.size(); ++i) {
        if (rotations[i].name == ctx.config_manager->GetConfig().active_rotation) {
            idx = (i + 1) % rotations.size();
            break;
        }
    }
    ctx.config_manager->GetConfig().active_rotation = rotations[idx].name;
    ApplyActiveRotation(ctx);
    LOG_INFO(std::format("Switched to rotation: {}", rotations[idx].name));
}

static void OpenEditor(AppContext& ctx) {
    if (!ctx.editor) return;
    if (!ctx.editor->IsOpen()) {
        ctx.editor->Show();
        ctx.playback_engine->Pause();
        if (ctx.file_watcher) ctx.file_watcher->Pause();
        LOG_INFO("Editor opened.");
    } else {
        ctx.editor->Hide();
        ctx.playback_engine->Play();
        if (ctx.file_watcher) ctx.file_watcher->Resume();
        LOG_INFO("Editor closed.");
    }
}

static void ReloadConfig(AppContext& ctx) {
    if (!ctx.config_manager) return;
    std::lock_guard<std::mutex> lock(ctx.config_mutex);
    if (ctx.config_manager->Load()) {
        ctx.mode = GetModeString(*ctx.config_manager);
        ApplyActiveRotation(ctx);
        SwitchKeyDetector(ctx);
        // 重新注册热键
        if (ctx.hotkey_manager && ctx.window) {
            ctx.hotkey_manager->UnregisterAll();
            ctx.hotkey_manager->RegisterFromConfig(
                ctx.config_manager->GetConfig().settings.at("hotkeys"), ctx.window->GetHwnd());
        }
        LOG_INFO("Config reloaded.");
    }
}

static void HandleHotkeyAction(AppContext& ctx, const std::string& action) {
    if (action == "toggle_visibility") {
        if (!ctx.window) return;
        if (ctx.visible.load()) {
            ctx.window->Hide();
        } else {
            ctx.window->Show();
        }
        ctx.visible.store(!ctx.visible.load());
    } else if (action == "play_pause") {
        if (ctx.playback_engine) ctx.playback_engine->PlayPause();
    } else if (action == "toggle_mode") {
        ToggleMode(ctx);
    } else if (action == "next_rotation") {
        NextRotation(ctx);
    } else if (action == "open_editor") {
        OpenEditor(ctx);
    } else if (action == "move_mode") {
        if (ctx.window) {
            bool next = !ctx.window->IsMoveMode();
            ctx.window->SetMoveMode(next);
        }
    } else if (action == "reload_config") {
        ReloadConfig(ctx);
    } else if (action == "quit") {
        ctx.running.store(false);
        if (ctx.window) PostMessageW(ctx.window->GetHwnd(), WM_CLOSE, 0, 0);
    }
}

constexpr UINT WM_USER_CONFIG_CHANGED = WM_APP + 42;
constexpr UINT_PTR kConfigReloadTimerId = 1001;
constexpr UINT kConfigReloadDebounceMs = 400;

static LRESULT CALLBACK OverlaySubclassProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam,
                                            UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    auto* ctx = reinterpret_cast<AppContext*>(dwRefData);
    if (msg == WM_USER_CONFIG_CHANGED) {
        if (!ctx || !ctx->file_watcher || ctx->file_watcher->IsPaused()) return 0;
        KillTimer(hWnd, kConfigReloadTimerId);
        SetTimer(hWnd, kConfigReloadTimerId, kConfigReloadDebounceMs, nullptr);
        return 0;
    }
    if (msg == WM_TIMER && wParam == kConfigReloadTimerId) {
        KillTimer(hWnd, kConfigReloadTimerId);
        if (ctx && ctx->file_watcher && !ctx->file_watcher->IsPaused()) {
            ReloadConfig(*ctx);
        }
        return 0;
    }
    if (msg == WM_NCDESTROY) {
        RemoveWindowSubclass(hWnd, OverlaySubclassProc, uIdSubclass);
    }
    return DefSubclassProc(hWnd, msg, wParam, lParam);
}

static void RenderLoop(AppContext& ctx) {
    using clock = std::chrono::steady_clock;
    auto last_time = clock::now();
    auto last_poll = clock::now();

    while (ctx.running.load(std::memory_order_acquire)) {
        auto now = clock::now();
        float dt_ms = std::chrono::duration<float, std::milli>(now - last_time).count();
        last_time = now;

        if (dt_ms < 0.0f) dt_ms = 0.0f;
        if (dt_ms > 100.0f) dt_ms = 100.0f;

        // 兜底热键检测（≤10Hz）
        if (std::chrono::duration<float, std::milli>(now - last_poll).count() >= 100.0f) {
            if (ctx.hotkey_manager) ctx.hotkey_manager->PollFallback();
            last_poll = now;
        }

        float wrong_flash = 0.0f;
        overlay::overlay::RenderState state{};

        {
            std::lock_guard<std::mutex> lock(ctx.config_mutex);

            const auto* rotation = GetActiveRotation(*ctx.config_manager);

            // 按键检测模式推进
            if (ModeIsKey(ctx.mode) && ctx.active_detector && rotation &&
                ctx.playback_engine &&
                ctx.playback_engine->GetState() == overlay::core::PlaybackState::Playing) {
                ctx.active_detector->Update();
                size_t step = ctx.playback_engine->CurrentStep();
                if (step < rotation->steps.size()) {
                    const std::string& expected = rotation->steps[step].key;
                    if (ctx.active_detector->WasKeyPressed(expected)) {
                        ctx.playback_engine->NextStep();
                    }
                    if (ctx.active_detector->IsWrongKeyPressed()) {
                        ctx.wrong_key_flash.store(true, std::memory_order_release);
                        ctx.wrong_flash_start = now;
                    }
                }
            }

            // 自动播放模式推进
            if (!ModeIsKey(ctx.mode) && ctx.playback_engine) {
                ctx.playback_engine->Update(dt_ms);
            }

            // 加载当前步骤所需图像
            LoadCurrentImages(ctx);

            // 计算错键闪烁强度（300ms）
            if (ctx.wrong_key_flash.load(std::memory_order_acquire)) {
                auto elapsed = std::chrono::duration<float, std::milli>(now - ctx.wrong_flash_start).count();
                if (elapsed >= 300.0f) {
                    ctx.wrong_key_flash.store(false, std::memory_order_release);
                } else {
                    wrong_flash = 1.0f - (elapsed / 300.0f);
                }
            }

            state.current_step = ctx.playback_engine ? ctx.playback_engine->CurrentStep() : 0;
            state.scale = GetScale(*ctx.config_manager);
            state.opacity = GetOpacity(*ctx.config_manager);
            state.move_mode = ctx.window ? ctx.window->IsMoveMode() : false;
            state.wrong_key_flash = wrong_flash;
            state.window_width = static_cast<float>(ctx.window ? ctx.window->GetClientWidth() : 800);
            state.window_height = static_cast<float>(ctx.window ? ctx.window->GetClientHeight() : 120);
            state.show_progress = GetBoolSetting(*ctx.config_manager, "display", "show_progress", false);
            if (rotation && !rotation->steps.empty() && ctx.playback_engine) {
                state.overall_progress = (static_cast<float>(ctx.playback_engine->CurrentStep()) +
                                          ctx.playback_engine->CurrentStepProgress()) /
                                         static_cast<float>(rotation->steps.size());
            }
            state.bg_image = ctx.bg_data ? &*ctx.bg_data : nullptr;
            state.avatar_image = ctx.avatar_data ? &*ctx.avatar_data : nullptr;

            ctx.renderer->BeginDraw();
            ctx.renderer->Clear({0.0f, 0.0f, 0.0f, 0.0f});
            ctx.visual_renderer->Render(state);
            ctx.renderer->EndDraw();
        }

        ctx.renderer->Present();

        // 渲染编辑器窗口（如果打开）
        if (ctx.editor && ctx.editor->IsOpen()) {
            ctx.editor->RenderFrame();
        }

        // 按键检测模式下不需要逐帧 16ms 高速渲染，可降低到 ~30Hz
        std::this_thread::sleep_for(ModeIsKey(ctx.mode) ? std::chrono::milliseconds(33)
                                                        : std::chrono::milliseconds(16));
    }
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

    AppContext ctx;
    ctx.hInstance = hInstance;
    ctx.exe_dir = exe_dir;

    overlay::core::ConfigManager config_manager(exe_dir);
    if (!config_manager.Load()) {
        LOG_ERROR("Failed to load config.");
        CoUninitialize();
        return 1;
    }
    ctx.config_manager = &config_manager;
    ctx.mode = GetModeString(config_manager);

    auto active = config_manager.GetActiveRotation();
    if (active) {
        LOG_INFO(std::format("Active rotation: {}", (*active)->name));
    }

    overlay::utils::ResourceLoader resource_loader;
    if (!resource_loader.Initialize()) {
        LOG_ERROR("Failed to initialize ResourceLoader.");
        CoUninitialize();
        return 1;
    }
    ctx.resource_loader = &resource_loader;

    overlay::utils::HotkeyManager hotkey_manager;
    ctx.hotkey_manager = &hotkey_manager;

    overlay::core::PlaybackEngine playback_engine;
    ctx.playback_engine = &playback_engine;

    overlay::editor::EditorWindow editor;
    ctx.editor = &editor;

    // 窗口尺寸
    float window_width = 800.0f;
    float window_height = 120.0f;
    overlay::overlay::Direct2DRenderer renderer;
    overlay::overlay::VisualRenderer visual_renderer(&renderer);
    ctx.renderer = &renderer;
    ctx.visual_renderer = &visual_renderer;

    float scale = GetScale(config_manager);
    if (active) {
        visual_renderer.SetRotation(*active);
        visual_renderer.ComputeWindowSize(scale, window_width, window_height);
    }

    // 创建 overlay 窗口
    overlay::overlay::OverlayWindow window;
    ctx.window = &window;
    if (!window.Create(hInstance, L"MingC Key Overlay",
                       100, 100,
                       static_cast<int>(window_width),
                       static_cast<int>(window_height))) {
        LOG_ERROR("Failed to create OverlayWindow.");
        CoUninitialize();
        return 1;
    }

    // 按键事件转发给事件驱动检测器
    window.SetKeyEventCallback([&ctx](UINT msg, WPARAM wParam, LPARAM lParam) {
        (void)lParam;
        if (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) {
            ctx.event_detector.OnKeyDown(static_cast<std::uint32_t>(wParam));
        } else if (msg == WM_KEYUP || msg == WM_SYSKEYUP) {
            ctx.event_detector.OnKeyUp(static_cast<std::uint32_t>(wParam));
        }
    });

    // 注册热键
    try {
        hotkey_manager.RegisterFromConfig(config_manager.GetConfig().settings.at("hotkeys"),
                                          window.GetHwnd());
    } catch (...) {
        LOG_ERROR("Failed to register hotkeys from config.");
    }

    hotkey_manager.SetActionCallback([&ctx](const std::string& action) {
        HandleHotkeyAction(ctx, action);
    });

    window.SetHotkeyMessageCallback([&hotkey_manager](WPARAM wParam) {
        hotkey_manager.OnHotkeyMessage(wParam);
    });

    window.Show();
    LOG_INFO("OverlayWindow shown.");

    HWND hwnd = window.GetHwnd();
    if (!renderer.Initialize(hwnd)) {
        LOG_ERROR("Failed to initialize Direct2DRenderer.");
        CoUninitialize();
        return 1;
    }
    renderer.Resize(static_cast<int>(window_width), static_cast<int>(window_height));

    // 初始化编辑器窗口（延迟到首次打开时再创建 D3D/ImGui 资源）
    editor.Initialize(hwnd, renderer.GetD3DDevice());
    editor.SetConfigManager(&config_manager);
    editor.SetApplyCallback([&ctx]() {
        if (ctx.file_watcher) ctx.file_watcher->Pause();
        auto resume_guard = [](void* p) {
            auto* c = static_cast<AppContext*>(p);
            if (c->file_watcher) c->file_watcher->Resume();
        };
        std::unique_ptr<void, decltype(resume_guard)> guard(&ctx, resume_guard);

        ApplyActiveRotation(ctx);
        SwitchKeyDetector(ctx);
        if (ctx.hotkey_manager && ctx.window) {
            ctx.hotkey_manager->UnregisterAll();
            try {
                ctx.hotkey_manager->RegisterFromConfig(
                    ctx.config_manager->GetConfig().settings.at("hotkeys"),
                    ctx.window->GetHwnd());
            } catch (...) {
                LOG_ERROR("Failed to re-register hotkeys after editor save.");
            }
        }
    });

    // 初始化播放引擎与按键检测器
    ApplyActiveRotation(ctx);
    SwitchKeyDetector(ctx);

    // 配置文件热加载
    overlay::utils::FileWatcher file_watcher;
    if (file_watcher.Start(config_manager.GetExeDirectory() / "profiles" / "default.json",
                           window.GetHwnd(), WM_USER_CONFIG_CHANGED)) {
        ctx.file_watcher = &file_watcher;
    }
    SetWindowSubclass(window.GetHwnd(), OverlaySubclassProc, 0,
                      reinterpret_cast<DWORD_PTR>(&ctx));

    // 启动自动关闭计时器（仅用于阶段验证，最终产品可移除）
    std::thread timer([hwnd]() {
        std::this_thread::sleep_for(5s);
        LOG_INFO("Auto-close timer elapsed, posting WM_CLOSE.");
        PostMessageW(hwnd, WM_CLOSE, 0, 0);
    });

    std::thread render_thread(RenderLoop, std::ref(ctx));

    window.RunMessageLoop();
    LOG_INFO("Message loop exited.");

    ctx.running.store(false, std::memory_order_release);
    LOG_INFO("Waiting for render thread...");
    render_thread.join();
    LOG_INFO("Render thread joined.");
    LOG_INFO("Waiting for timer thread...");
    timer.join();
    LOG_INFO("Timer thread joined.");

    if (ctx.active_detector == &ctx.polling_detector) {
        ctx.polling_detector.Stop();
    }
    LOG_INFO("Unregistering hotkeys...");
    hotkey_manager.UnregisterAll();
    LOG_INFO("Shutting down editor...");
    editor.Shutdown();
    renderer.Shutdown();
    window.Destroy();
    resource_loader.Shutdown();

    LOG_INFO("Application exiting.");
    CoUninitialize();
    return 0;
}
