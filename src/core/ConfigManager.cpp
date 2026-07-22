#include "core/ConfigManager.h"

#include "utils/Logger.h"
#include "utils/TextEncoding.h"

#include <filesystem>
#include <fstream>

namespace overlay::core {

namespace fs = std::filesystem;
using json = nlohmann::json;

ConfigManager::ConfigManager(fs::path exe_dir) : exe_dir_(std::move(exe_dir)) {
    config_path_ = exe_dir_ / "profiles" / "default.json";
    CreateDefaultConfig();
}

void ConfigManager::CreateDefaultConfig() {
    config_.active_rotation = "常规循环·散秧维";
    config_.settings = json::object();
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
        config_.settings = j.value("settings", json::object());
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
        std::ofstream file(config_path_);
        if (!file.is_open()) return false;
        file << j.dump(2);
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
