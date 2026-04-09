#pragma once

#include "game/battle/battle_types.h"

#include <vector>

namespace game::data {
struct EnemyData;
class RpgCatalog;
} // namespace game::data

namespace game::battle {

/// @brief 为当前敌方回合生成最小可执行行动的纯逻辑 helper。
class BattleAiPlanner final {
public:
    /// @brief 基于敌人目录动作表规划敌方行动。
    [[nodiscard]] static BattleAction planEnemyAction(const BattleUnit& actor,
                                                      const game::data::EnemyData& enemy,
                                                      const std::vector<BattleUnit>& units,
                                                      const game::data::RpgCatalog& rpg_catalog);

    /// @brief 在缺少目录来源或有效技能时生成基础 fallback 行动。
    [[nodiscard]] static BattleAction planFallbackAction(const BattleUnit& actor,
                                                         const std::vector<BattleUnit>& units);
};

} // namespace game::battle
