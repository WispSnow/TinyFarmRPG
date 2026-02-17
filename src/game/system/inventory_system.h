#pragma once

#include "game/component/inventory_component.h"
#include "game/data/item_catalog.h"
#include "game/defs/commands.h"
#include "game/defs/events.h"
#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>

namespace game::system {

class InventorySystem final {
    entt::registry& registry_;
    entt::dispatcher& dispatcher_;
    game::data::ItemCatalog& catalog_;

public:
    InventorySystem(entt::registry& registry, entt::dispatcher& dispatcher, game::data::ItemCatalog& catalog);
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
    void emitChanged(entt::entity target, const std::vector<game::defs::InventorySlotUpdate>& diff, bool full_sync, int active_page);
};

} // namespace game::system
