#pragma once

#include "game/defs/constants.h"
#include "game/defs/crop_defs.h"
#include <entt/entity/entity.hpp>

namespace game::component {

struct ActorComponent {
    float speed_{0.0f};
    game::defs::Tool tool_{game::defs::Tool::None};
    game::defs::CropType hold_seed_{game::defs::CropType::Unknown};  ///< @brief 当前持有的种子类型，Unknown表示未持有种子
    entt::id_type hold_seed_item_id_{entt::null};                    ///< @brief 当前持有种子的物品ID
    int hold_seed_inventory_slot_{-1};                               ///< @brief 当前持有种子的物品栏槽位
};

} // namespace game::component
