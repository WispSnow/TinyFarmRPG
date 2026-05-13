#pragma once

#include "game/component/hotbar_component.h"
#include "game/component/inventory_component.h"
#include "game/defs/commands.h"
#include "game/defs/events.h"
#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>
#include <vector>

namespace game::data {
class ItemCatalog;
}

namespace game::system {

class HotbarSystem final {
    entt::registry& registry_;
    entt::dispatcher& dispatcher_;
    const game::data::ItemCatalog* item_catalog_{nullptr};

public:
    HotbarSystem(entt::registry& registry,
                 entt::dispatcher& dispatcher,
                 const game::data::ItemCatalog* item_catalog = nullptr);
    ~HotbarSystem();

    void subscribe();
    void unsubscribe();

private:
    void onBind(const game::defs::HotbarBindCommand& evt);
    void onUnbind(const game::defs::HotbarUnbindCommand& evt);
    void onSync(const game::defs::HotbarSyncCommand& evt);
    void onInventoryChanged(const game::defs::InventoryChanged& evt);

    bool validateTarget(entt::entity target) const;
    void emitChanged(entt::entity target, const std::vector<game::defs::HotbarSlotUpdate>& updates, bool full_sync);
    std::vector<game::defs::HotbarSlotUpdate> collectAll(entt::entity target) const;
};

} // namespace game::system
