#pragma once

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>

#include <string>

namespace game::component {

struct EnemyEncounterComponent {
    std::string troop_id_{};
    entt::id_type troop_id_hash_{entt::null};
    std::string battle_background_id_{};
    int encounter_id_{0};
    bool respawn_on_map_reload_{true};
    bool defeated_{false};
    glm::vec2 home_position_{0.0f, 0.0f};
    float cooldown_seconds_{1.0f};
    float cooldown_timer_{0.0f};
    bool engaged_{false};
};

} // namespace game::component
