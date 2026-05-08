#pragma once

#include "game/battle/battle_types.h"

#include <random>
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
    ///
    /// 单体攻击目标在存活对手中随机抽取；治疗类技能仍按缺失程度挑选目标。
    /// @param random_engine 可选随机源；未提供时使用默认线程局部随机引擎。
    [[nodiscard]] static BattleAction planEnemyAction(const BattleUnit& actor,
                                                      const game::data::EnemyData& enemy,
                                                      const std::vector<BattleUnit>& units,
                                                      const game::data::RpgCatalog& rpg_catalog,
                                                      std::mt19937* random_engine = nullptr);

    /// @brief 在缺少目录来源或有效技能时生成基础 fallback 行动。
    ///
    /// fallback 普攻同样会在存活对手中随机抽取目标。
    /// @param random_engine 可选随机源；未提供时使用默认线程局部随机引擎。
    [[nodiscard]] static BattleAction planFallbackAction(const BattleUnit& actor,
                                                         const std::vector<BattleUnit>& units,
                                                         std::mt19937* random_engine = nullptr);
};

} // namespace game::battle
