#pragma once

#include "game/defs/constants.h"
#include "game/defs/crop_defs.h"

#include <entt/core/fwd.hpp>
#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>

namespace game::defs {

struct UseToolEvent {
    Tool tool_{Tool::None};
    glm::vec2 world_pos_{0.0f, 0.0f};
};

struct SwitchToolEvent {
    Tool tool_{Tool::None};
};

struct SwitchSeedEvent {
    CropType seed_type_{CropType::Unknown};           ///< @brief 要切换到的种子类型，Unknown表示取消种子
    entt::id_type seed_item_id_{entt::null};          ///< @brief 当前种子物品ID
    int inventory_slot_index_{-1};                    ///< @brief 当前种子所在的物品栏槽位
};

struct UseSeedEvent {
    CropType seed_type_{CropType::Unknown};
    glm::vec2 world_pos_{0.0f, 0.0f};
    entt::entity source{entt::null};                  ///< @brief 发起者（通常是玩家）
    entt::id_type seed_item_id_{entt::null};          ///< @brief 被使用的种子物品ID
    int inventory_slot_index_{-1};                    ///< @brief 种子所在的物品栏槽位
};

} // namespace game::defs
