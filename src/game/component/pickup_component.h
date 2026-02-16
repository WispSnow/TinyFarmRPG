#pragma once

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>

namespace game::component {

struct PickupComponent {
    entt::id_type item_id_{entt::null};
    int count_{1};
    float pickup_delay_{0.0f};
};

struct ScatterMotionComponent {
    glm::vec2 start_pos_{};
    glm::vec2 target_pos_{};
    float elapsed_{0.0f};
    float duration_{0.22f};
    float arc_height_{6.0f};
};

} // namespace game::component
