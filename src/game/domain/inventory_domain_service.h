#pragma once

#include "game/defs/events.h"

#include <entt/core/fwd.hpp>
#include <entt/entity/fwd.hpp>
#include <entt/signal/fwd.hpp>

#include <vector>

namespace game::data {
class ItemCatalog;
}

namespace game::domain {

struct InventoryMutationResult {
    entt::entity target{entt::null};
    std::vector<game::defs::InventorySlotUpdate> changed_slots{};
    int active_page{0};
    int accepted{0};
    int rejected{0};
};

class InventoryDomainService final {
    entt::registry& registry_;
    entt::dispatcher& dispatcher_;
    game::data::ItemCatalog& catalog_;

public:
    InventoryDomainService(entt::registry& registry, entt::dispatcher& dispatcher, game::data::ItemCatalog& catalog);

    [[nodiscard]] bool ensureInventory(entt::entity target);

    [[nodiscard]] InventoryMutationResult addItem(entt::entity target,
                                                  entt::id_type item_id,
                                                  int count,
                                                  int preferred_slot_index = -1);

    [[nodiscard]] InventoryMutationResult removeItem(entt::entity target,
                                                     entt::id_type item_id,
                                                     int count,
                                                     int slot_index = -1);

private:
    void emitChanged(entt::entity target,
                     const std::vector<game::defs::InventorySlotUpdate>& diff,
                     int active_page,
                     bool from_add) const;
};

} // namespace game::domain
