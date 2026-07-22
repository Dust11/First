#include "core/ConfigManager.h"
#include "utils/Logger.h"
#include "utils/TextEncoding.h"

#include <windows.h>
#include <filesystem>
#include <format>

namespace fs = std::filesystem;

static fs::path GetExeDirectory() {
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    return fs::path(path).parent_path();
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int) {
    fs::path exe_dir = GetExeDirectory();
    overlay::utils::Logger::Instance().Init(exe_dir / "overlay.log");
    LOG_INFO("Application started.");

    overlay::core::ConfigManager config_manager(exe_dir);
    if (!config_manager.Load()) {
        LOG_ERROR("Failed to load config.");
        return 1;
    }

    auto active = config_manager.GetActiveRotation();
    if (active) {
        LOG_INFO(std::format("Active rotation: {}", (*active)->name));
    }

    LOG_INFO("Application exiting.");
    return 0;
}
