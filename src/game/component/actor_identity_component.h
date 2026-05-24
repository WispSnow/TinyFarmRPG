#pragma once

#include <entt/core/fwd.hpp>
#include <entt/entity/entity.hpp>

#include <string>

namespace game::component {

/// @brief Stable script-facing identity for map actors and NPC-like entities.
struct ActorIdentityComponent {
    std::string actor_id_{};
    entt::id_type actor_id_hash_{entt::null};
    std::string blueprint_id_{};
};

} // namespace game::component
