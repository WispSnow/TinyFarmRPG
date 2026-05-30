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
    int accepted{0};
    int rejected{0};
};

class InventoryDomainService final {
    entt::registry& registry_;
    entt::dispatcher& dispatcher_;
    const game::data::ItemCatalog& catalog_;

public:
    InventoryDomainService(entt::registry& registry, entt::dispatcher& dispatcher, const game::data::ItemCatalog& catalog);

    [[nodiscard]] bool ensureInventory(entt::entity target);

    [[nodiscard]] InventoryMutationResult addItem(entt::entity target,
                                                  entt::id_type item_id,
                                                  int count,
                                                  int preferred_slot_index = -1);

    [[nodiscard]] InventoryMutationResult removeItem(entt::entity target,
                                                     entt::id_type item_id,
                                                     int count,
                                                     int slot_index = -1);

    [[nodiscard]] bool moveItem(entt::entity target, int from_slot, int to_slot, bool allow_merge = true);
    [[nodiscard]] bool sortInventory(entt::entity target);

private:
    void emitChanged(entt::entity target,
                     const std::vector<game::defs::InventorySlotUpdate>& diff,
                     bool from_add,
                     bool full_sync = false,
                     game::defs::InventoryMoveKind move_kind = game::defs::InventoryMoveKind::None,
                     int move_from_slot = -1,
                     int move_to_slot = -1) const;
};

} // namespace game::domain
