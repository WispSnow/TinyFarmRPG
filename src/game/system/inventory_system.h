#pragma once

#include "game/defs/commands.h"
#include "game/defs/events.h"

#include <entt/entity/fwd.hpp>
#include <entt/signal/fwd.hpp>

namespace game::data {
class ItemCatalog;
}

namespace game::domain {
class InventoryDomainService;
}

namespace game::system {

class InventorySystem final {
    entt::registry& registry_;
    entt::dispatcher& dispatcher_;
    game::data::ItemCatalog& catalog_;
    game::domain::InventoryDomainService& inventory_domain_service_;

public:
    InventorySystem(entt::registry& registry,
                    entt::dispatcher& dispatcher,
                    game::data::ItemCatalog& catalog,
                    game::domain::InventoryDomainService& inventory_domain_service);
    ~InventorySystem();

    void subscribe();
    void unsubscribe();

private:
    void onAddItem(const game::defs::AddItemCommand& evt);
    void onRemoveItem(const game::defs::RemoveItemCommand& evt);
    void onSync(const game::defs::InventorySyncCommand& evt);
    void onMoveItem(const game::defs::InventoryMoveCommand& evt);
    void onSetActivePage(const game::defs::InventorySetActivePageCommand& evt);

    bool ensureInventory(entt::entity target);
    void emitChanged(entt::entity target,
                     const std::vector<game::defs::InventorySlotUpdate>& diff,
                     bool full_sync,
                     int active_page,
                     game::defs::InventoryMoveKind move_kind = game::defs::InventoryMoveKind::None,
                     int move_from_slot = -1,
                     int move_to_slot = -1);
};

} // namespace game::system
