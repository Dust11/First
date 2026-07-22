#include "core/TeamRotation.h"

namespace overlay::core {

void NormalizeTeamRotation(TeamRotation& rotation) {
    if (rotation.stages.empty()) {
        rotation.stages.push_back({"准备", "", 0});
    }
}

} // namespace overlay::core
