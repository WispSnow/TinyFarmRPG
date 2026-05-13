#pragma once

#include "system_helpers.h"
#include "game/defs/commands.h"

#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>

namespace game::data {
class ItemCatalog;
class RpgCatalog;
}

namespace game::domain {
class InventoryDomainService;
}

namespace game::system {

class ItemUseSystem final {
    entt::registry& registry_;
    entt::dispatcher& dispatcher_;
    game::data::ItemCatalog& catalog_;
    const game::data::RpgCatalog* rpg_catalog_{nullptr};
    game::domain::InventoryDomainService& inventory_domain_service_;

    game::system::helpers::NotificationTimer notification_{};

public:
    ItemUseSystem(entt::registry& registry,
                  entt::dispatcher& dispatcher,
                  game::data::ItemCatalog& catalog,
                  game::domain::InventoryDomainService& inventory_domain_service,
                  const game::data::RpgCatalog* rpg_catalog = nullptr);
    ~ItemUseSystem();

    void update(float delta_time);

private:
    void onUseItem(const game::defs::UseItemCommand& evt);
    void updateNotification(float delta_time);
};

} // namespace game::system
