#pragma once

#include "core/TeamRotation.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <optional>
#include <string>

namespace overlay::core {

struct AppConfig {
    std::string active_rotation;
    std::vector<TeamRotation> rotations;
    nlohmann::json settings;
};

class ConfigManager {
public:
    explicit ConfigManager(std::filesystem::path exe_dir);

    bool Load();
    bool Save();
    AppConfig& GetConfig() { return config_; }
    const AppConfig& GetConfig() const { return config_; }
    std::optional<TeamRotation*> GetActiveRotation();
    const std::filesystem::path& GetExeDirectory() const { return exe_dir_; }

private:
    std::filesystem::path exe_dir_;
    std::filesystem::path config_path_;
    AppConfig config_;

    void CreateDefaultConfig();
};

} // namespace overlay::core
