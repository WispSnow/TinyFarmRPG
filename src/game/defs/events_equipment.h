#pragma once

#include "game/data/rpg_types.h"

#include <entt/core/fwd.hpp>
#include <entt/entity/entity.hpp>

#include <string>
#include <vector>

namespace game::defs {

struct EquipmentSlotUpdate {
    std::string actor_id{};
    game::data::EquipmentSlotId slot{game::data::EquipmentSlotId::Unknown};
    entt::id_type item_id{entt::null};
};

struct EquipmentChanged {
    entt::entity player{entt::null};
    std::string actor_id{};
    std::vector<EquipmentSlotUpdate> slots{};
    bool full_sync{false};
};

} // namespace game::defs
