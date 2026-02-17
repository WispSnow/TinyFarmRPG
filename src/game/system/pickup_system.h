#pragma once

#include <entt/entity/fwd.hpp>
#include <entt/signal/fwd.hpp>

namespace game::domain {
class InventoryDomainService;
}

namespace game::system {

class PickupSystem final {
    entt::registry& registry_;
    entt::dispatcher& dispatcher_;
    game::domain::InventoryDomainService& inventory_domain_service_;

public:
    PickupSystem(entt::registry& registry,
                 entt::dispatcher& dispatcher,
                 game::domain::InventoryDomainService& inventory_domain_service);
    ~PickupSystem() = default;

    void update(float delta_time);
};

} // namespace game::system
