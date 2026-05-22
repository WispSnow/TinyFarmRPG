#pragma once

#include "game/data/rpg_types.h"

#include <entt/entity/entity.hpp>

#include <string>

namespace game::defs {

struct EquipItemCommand {
    entt::entity player{entt::null};
    std::string actor_id{};
    int inventory_slot_index{-1};
    game::data::EquipmentSlotId target_slot{game::data::EquipmentSlotId::Unknown};
};

struct UnequipItemCommand {
    entt::entity player{entt::null};
    std::string actor_id{};
    game::data::EquipmentSlotId slot{game::data::EquipmentSlotId::Unknown};
    int preferred_inventory_slot{-1};
};

} // namespace game::defs
