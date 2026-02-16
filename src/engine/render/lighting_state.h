#pragma once

#include "engine/utils/defs.h"
#include <entt/core/fwd.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <unordered_map>

namespace engine::render {

struct DirectionalLightState {
    bool enabled{false};
    glm::vec2 direction{0.0f, -1.0f};
    engine::utils::DirectionalLightOptions options{};
};

struct GlobalLightingState {
    glm::vec3 ambient{1.0f, 1.0f, 1.0f};
    std::unordered_map<entt::id_type, DirectionalLightState> directional_lights{};
};

} // namespace engine::render
