#pragma once

#include "game/battle/battle_reward_resolver.h"
#include "game/battle/battle_types.h"

#include <entt/core/fwd.hpp>

#include <optional>
#include <unordered_map>
#include <vector>

namespace game::defs {

/// @brief 战斗场景退出时发出的结算事件。
///
/// GameScene 通过该事件恢复探索流程并处理未来的经验、掉落、任务推进等结算逻辑。
struct BattleEndedEvent {
    game::battle::BattleOutcome outcome{game::battle::BattleOutcome::Ongoing};    ///< 战斗最终结果。
    std::vector<game::battle::BattleUnit> final_units{};                          ///< 战斗结束时的完整单位状态副本。
    std::unordered_map<entt::id_type, int> remaining_item_stocks{};                ///< 战斗结束时剩余的战斗内道具库存。
    std::optional<game::battle::BattleRewardSummary> reward_summary{};             ///< Victory 时由 BattleScene 结算出的奖励摘要，避免写回阶段重复 roll 掉落。
};

} // namespace game::defs
