#include "core/ConfigManager.h"

#include "utils/Logger.h"
#include "utils/TextEncoding.h"

#include <filesystem>
#include <fstream>
#include <windows.h>

namespace overlay::core {

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace {

json CreateDefaultSettings() {
    json settings;
    settings["display"] = {
        {"opacity", 0.85},
        {"scale", 1.0},
        {"position", {{"x", 100}, {"y", 100}}},
        {"visible_keys", {{"before", 3}, {"after", 6}}},
        {"avatar_size", 56},
        {"stage_bar_height", 28},
        {"key_style",
         {{"key_width", 90},
          {"key_height", 44},
          {"spacing", 10},
          {"border_radius", 8},
          {"active_color", "#F3F4F6"},
          {"done_color", "#4B5563"},
          {"pending_color", "#1F2937"},
          {"active_text_color", "#111827"},
          {"text_color", "#F3F4F6"},
          {"font_size", 16},
          {"skill_name_font_size", 14},
          {"icon_size", 20},
          {"arrow_color", "#9CA3AF"}}},
        {"mode", "auto"},
        {"loop", true},
        {"show_progress", true},
        {"show_stage_bar", true},
        {"show_avatar", true},
        {"show_arrows", true}};
    settings["hotkeys"] = {
        {"toggle_visibility", "Ctrl+Shift+H"},
        {"play_pause", "Ctrl+Shift+Space"},
        {"toggle_mode", "Ctrl+Shift+P"},
        {"next_rotation", "Ctrl+Shift+N"},
        {"open_editor", "Ctrl+Shift+E"},
        {"move_mode", "Ctrl+Shift+M"},
        {"reload_config", "Ctrl+Shift+R"},
        {"quit", "Ctrl+Shift+Q"}};
    settings["input"] = {
        {"poll_hz", 60},
        {"foreground_only", true},
        {"target_process", "Client-Win64-Shipping.exe"},
        {"wrong_key_flash", true},
        {"timeout_skip_ms", 0}};
    return settings;
}

} // namespace

ConfigManager::ConfigManager(fs::path exe_dir) : exe_dir_(std::move(exe_dir)) {
    config_path_ = exe_dir_ / "profiles" / "default.json";
    CreateDefaultConfig();
}

void ConfigManager::CreateDefaultConfig() {
    config_.active_rotation = "常规循环·散秧维";
    config_.settings = CreateDefaultSettings();
    config_.rotations.clear();

    TeamRotation rotation;
    rotation.name = "常规循环·散秧维";
    rotation.background_image = "assets/bg_01.png";
    rotation.characters = {
        {"散华", "assets/avatar_sanhua.png", "#8B5CF6"},
        {"秧秧", "assets/avatar_yangyang.png", "#3B82F6"},
        {"维里奈", "assets/avatar_verina.png", "#22C55E"},
    };
    rotation.stages = {
        {"攒能量", "", 0},
        {"满能量放强化重击", "#EF4444", 4},
        {"重击飞起来重新攒能量", "", 6},
    };
    rotation.steps = {
        {"散华", "LButton", "普攻*3", "mouse_left", 1200},
        {"散华", "LButton", "长按普攻", "mouse_left_hold", 1500},
        {"散华", "E", "技能", "keyboard", 2000},
        {"散华", "LButton", "普攻*3", "mouse_left", 1200},
        {"散华", "LButton", "强化重击", "mouse_left_hold", 1500},
        {"散华", "R", "大招", "keyboard", 2500},
        {"秧秧", "Q", "声骸", "keyboard", 1800},
    };
    NormalizeTeamRotation(rotation);
    config_.rotations.push_back(std::move(rotation));
}

bool ConfigManager::Load() {
    try {
        if (!fs::exists(config_path_)) {
            LOG_INFO("Config not found, creating default.");
            return Save();
        }
        std::ifstream file(config_path_);
        if (!file.is_open()) {
            LOG_ERROR("Failed to open config file.");
            return false;
        }
        json j;
        file >> j;
        config_.active_rotation = j.at("active_rotation").get<std::string>();
        config_.rotations = j.at("team_rotations").get<std::vector<TeamRotation>>();
        config_.settings = j.value("settings", CreateDefaultSettings());
        for (auto& rot : config_.rotations) {
            NormalizeTeamRotation(rot);
        }
        LOG_INFO("Config loaded.");
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR(std::format("Config load failed: {}", e.what()));
        return false;
    }
}

bool ConfigManager::Save() {
    try {
        fs::create_directories(config_path_.parent_path());
        json j;
        j["active_rotation"] = config_.active_rotation;
        j["team_rotations"] = config_.rotations;
        j["settings"] = config_.settings;
        const std::string content = j.dump(2);

        // 原子写入：同目录写 tmp → FlushFileBuffers → MoveFileExW 替换
        const fs::path tmp_path = config_path_.wstring() + L".tmp";
        {
            std::ofstream file(tmp_path, std::ios::out | std::ios::trunc | std::ios::binary);
            if (!file.is_open()) {
                LOG_ERROR("Failed to open temporary config file.");
                return false;
            }
            file << content;
            file.flush();
        }

        HANDLE hFile = CreateFileW(
            tmp_path.wstring().c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile != INVALID_HANDLE_VALUE) {
            FlushFileBuffers(hFile);
            CloseHandle(hFile);
        }

        const BOOL moved = MoveFileExW(
            tmp_path.wstring().c_str(), config_path_.wstring().c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
        if (!moved) {
            LOG_ERROR(std::format("MoveFileExW failed, error={}", GetLastError()));
            fs::remove(tmp_path);
            return false;
        }

        LOG_INFO("Config saved.");
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR(std::format("Config save failed: {}", e.what()));
        return false;
    }
}

std::optional<TeamRotation*> ConfigManager::GetActiveRotation() {
    for (auto& rot : config_.rotations) {
        if (rot.name == config_.active_rotation) return &rot;
    }
    if (!config_.rotations.empty()) {
        config_.active_rotation = config_.rotations[0].name;
        return &config_.rotations[0];
    }
    return std::nullopt;
}

} // namespace overlay::core
