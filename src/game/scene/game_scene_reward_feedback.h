#pragma once

#include "game/battle/battle_reward_resolver.h"

#include <string>
#include <vector>

namespace game::data {
class ItemCatalog;
}

namespace game::scene {

/// @brief 单个掉落条目的实际写回结果。
struct BattleRewardWritebackItemResult {
    game::battle::BattleRewardItemDrop drop{};   ///< 原始奖励摘要中的掉落条目。
    int accepted{0};                             ///< 实际成功写入背包的数量。
    int rejected{0};                             ///< 实际未能写入背包的数量。
};

/// @brief 将奖励写回结果格式化为玩家可见的最小反馈文本。
[[nodiscard]] std::string formatRewardFeedback(
    int gold_written_back,
    const std::vector<BattleRewardWritebackItemResult>& item_results,
    const game::data::ItemCatalog* item_catalog);

} // namespace game::scene
