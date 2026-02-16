#pragma once

#include <entt/entity/fwd.hpp>
#include <random>

namespace game::system {

class NPCWanderSystem {
    entt::registry& registry_;

public:
    explicit NPCWanderSystem(entt::registry& registry);
    void update(float delta_time);
};

} // namespace game::system
