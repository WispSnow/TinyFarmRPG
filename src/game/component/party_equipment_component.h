#pragma once

#include "game/data/rpg_types.h"

#include <entt/entity/entity.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>

namespace game::component {

struct EquipmentSlotIdHash {
    [[nodiscard]] std::size_t operator()(game::data::EquipmentSlotId value) const noexcept {
        return static_cast<std::size_t>(value);
    }
};

/// @brief 单个 actor 当前穿戴的装备。item id 对应 ItemCatalog / equipment.json 的物品 id。
struct ActorEquipmentLoadout {
    std::unordered_map<game::data::EquipmentSlotId, entt::id_type, EquipmentSlotIdHash> equipped_item_ids_{};
};

/// @brief 玩家队伍装备状态。挂在玩家实体上，因为队友不一定在当前地图中有实体。
struct PartyEquipmentComponent {
    std::unordered_map<std::string, ActorEquipmentLoadout> loadouts_by_actor_id_{};
    std::uint32_t revision_{0};
};

} // namespace game::component
