#pragma once

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace overlay::core {

struct CharacterInfo {
    std::string name;
    std::string avatar_image;
    std::string theme_color;
};

struct StageMarker {
    std::string label;
    std::string color;
    size_t start_step = 0;
};

struct KeyStep {
    std::string character;
    std::string key;
    std::string skill_name;
    std::string key_icon;
    int duration_ms = 1000;
};

struct TeamRotation {
    std::string name;
    std::string background_image;
    std::vector<CharacterInfo> characters;
    std::vector<StageMarker> stages;
    std::vector<KeyStep> steps;
};

void NormalizeTeamRotation(TeamRotation& rotation);

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(CharacterInfo, name, avatar_image, theme_color)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(StageMarker, label, color, start_step)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(KeyStep, character, key, skill_name, key_icon, duration_ms)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(TeamRotation, name, background_image, characters, stages, steps)

} // namespace overlay::core
