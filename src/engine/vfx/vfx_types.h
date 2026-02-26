#pragma once

#include "engine/utils/math.h"

#include <entt/entity/fwd.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>

#include <string>

namespace engine::vfx {

inline constexpr entt::id_type kInvalidVfxEffectId{0};

struct VfxPlayRequest {
    entt::id_type effect_id{kInvalidVfxEffectId};
    std::string effect_path{};
    glm::vec2 world_position{0.0f, 0.0f};
    float z{0.0f};
    float scale{1.0f};
    bool loop{false};
};

struct VfxRenderContext {
    glm::mat4 view_projection{1.0f};
    glm::vec2 logical_size{0.0f, 0.0f};
    glm::vec2 camera_position{0.0f, 0.0f};
    float camera_zoom{1.0f};
    float camera_rotation{0.0f};
    engine::utils::Rect viewport_pixels{};
};

} // namespace engine::vfx
