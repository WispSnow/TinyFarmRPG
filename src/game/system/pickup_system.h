#pragma once

#include <entt/entity/fwd.hpp>
#include <entt/signal/fwd.hpp>

namespace game::system {

class PickupSystem final {
    entt::registry& registry_;
    entt::dispatcher& dispatcher_;

public:
    PickupSystem(entt::registry& registry, entt::dispatcher& dispatcher);
    ~PickupSystem() = default;

    void update(float delta_time);
};

} // namespace game::system

