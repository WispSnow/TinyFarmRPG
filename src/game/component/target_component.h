#pragma once
#include "engine/utils/math.h"
#include "game/defs/constants.h"

namespace game::component {

struct TargetComponent {
    glm::vec2 position_{};
    engine::utils::FColor color_{defs::CURSOR_COLOR};
};

} // namespace game::component