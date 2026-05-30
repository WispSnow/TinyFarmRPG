#pragma once

#include "game/data/rpg_types.h"

#include <entt/entity/entity.hpp>
#include <entt/signal/fwd.hpp>

#include <string>

namespace game::data {
class ItemCatalog;
class RpgCatalog;
}

namespace game::domain {

struct EquipmentMutationResult {
    bool success{false};
    std::string message{};
};

class EquipmentDomainService final {
    entt::registry& registry_;
    entt::dispatcher& dispatcher_;
    const game::data::RpgCatalog& rpg_catalog_;
    const game::data::ItemCatalog& item_catalog_;

public:
    EquipmentDomainService(entt::registry& registry,
                           entt::dispatcher& dispatcher,
                           const game::data::RpgCatalog& rpg_catalog,
                           const game::data::ItemCatalog& item_catalog);

    [[nodiscard]] EquipmentMutationResult equipItem(entt::entity player,
                                                    const std::string& actor_id,
                                                    int inventory_slot_index,
                                                    game::data::EquipmentSlotId target_slot);

    [[nodiscard]] EquipmentMutationResult unequipItem(entt::entity player,
                                                      const std::string& actor_id,
                                                      game::data::EquipmentSlotId slot,
                                                      int preferred_inventory_slot = -1);

private:
    void emitChanged(entt::entity player,
                     const std::string& actor_id,
                     game::data::EquipmentSlotId slot,
                     entt::id_type item_id) const;
};

} // namespace game::domain
