#pragma once

#include <entt/entity/fwd.hpp>

namespace game::system {

class AnimalBehaviorSystem {
    entt::registry& registry_;

public:
    explicit AnimalBehaviorSystem(entt::registry& registry);
    void update(float delta_time);
};

} // namespace game::system
