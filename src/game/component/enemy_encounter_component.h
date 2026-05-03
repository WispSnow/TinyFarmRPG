#pragma once

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>

#include <string>

namespace game::component {

struct EnemyEncounterComponent {
    std::string troop_id_{};
    entt::id_type troop_id_hash_{entt::null};
    int encounter_id_{0};
    bool once_{false};
    bool defeated_{false};
    glm::vec2 home_position_{0.0f, 0.0f};
    float cooldown_seconds_{1.0f};
    float cooldown_timer_{0.0f};
    bool engaged_{false};
};

} // namespace game::component
