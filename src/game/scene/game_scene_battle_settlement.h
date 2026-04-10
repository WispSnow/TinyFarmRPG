#pragma once

#include "game/defs/events.h"

#include <entt/entity/fwd.hpp>
#include <entt/signal/fwd.hpp>

#include <unordered_map>

namespace game::runtime {
struct GameRuntimeServices;
}

namespace game::scene {

/// @brief 处理战斗结束后的物品库存写回、胜利奖励写回与最小反馈。
void processBattleEndedForGameScene(entt::registry& registry,
                                    entt::dispatcher& dispatcher,
                                    game::runtime::GameRuntimeServices* services,
                                    std::unordered_map<entt::id_type, int>& active_battle_initial_item_stocks,
                                    bool& has_active_battle_item_stocks,
                                    const game::defs::BattleEndedEvent& evt);

} // namespace game::scene
