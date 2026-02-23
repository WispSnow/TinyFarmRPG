#pragma once

#include "engine/system/deferred_commands.h"

#include <entt/entity/fwd.hpp>

namespace game::data {
struct GameTime;
}

namespace game::system {

class AnimalBehaviorSystem {
    entt::registry& registry_;

public:
    explicit AnimalBehaviorSystem(entt::registry& registry);
    void update(float delta_time,
                const game::data::GameTime* game_time,
                engine::system::DeferredCommands& deferred);
};

} // namespace game::system
