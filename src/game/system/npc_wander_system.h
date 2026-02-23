#pragma once

#include "engine/system/deferred_commands.h"

#include <entt/entity/fwd.hpp>

namespace game::system {

class NPCWanderSystem {
    entt::registry& registry_;

public:
    explicit NPCWanderSystem(entt::registry& registry);
    void update(float delta_time, engine::system::DeferredCommands& deferred);
};

} // namespace game::system
