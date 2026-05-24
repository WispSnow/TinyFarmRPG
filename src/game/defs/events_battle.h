#pragma once

#include "game/battle/battle_reward_resolver.h"
#include "game/battle/battle_types.h"

#include <entt/core/fwd.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace game::defs {

/// @brief 探索流程确认进入战斗场景时发出的开始事件。
struct BattleStartedEvent {
    std::vector<std::string> actor_ids{};
    std::string troop_id{};
    std::string battle_background_id{};
    bool from_encounter{false};
    int encounter_id{0};
};

/// @brief BattleScene 进入一个可行动单位的回合时发出的脚本观察事件。
struct BattleTurnStartedEvent {
    game::battle::BattleUnit unit{};                 ///< 当前行动者的状态副本。
    std::uint32_t round_index{0};                    ///< BattleSession 当前轮次，从 1 开始。
};

/// @brief BattleScene 成功结算一个行动后发出的脚本观察事件。
struct BattleTurnEndedEvent {
    game::battle::BattleUnit unit{};                 ///< 行动者结算后的状态副本。
    game::battle::BattleActionResult result{};       ///< 行动结算结果。
    std::uint32_t round_index{0};                    ///< 行动开始时所在轮次。
};

/// @brief 单位在一次行动结算后从存活变为死亡时发出的脚本观察事件。
struct BattleUnitDiedEvent {
    game::battle::BattleUnit unit{};                         ///< 死亡单位结算后的状态副本。
    game::battle::BattleUnitId source_unit_id{0};             ///< 造成死亡的行动者单位 id。
    game::battle::BattleActionType source_action_type{game::battle::BattleActionType::EndTurn};
    std::string skill_id{};                                   ///< 技能行动时的技能 id；其他行动为空。
    std::string item_id{};                                    ///< 道具行动时的道具 id；其他行动为空。
    std::uint32_t round_index{0};                             ///< 行动开始时所在轮次。
};

/// @brief 单位成功使用技能时发出的脚本观察事件。
struct BattleSkillUsedEvent {
    game::battle::BattleUnit unit{};                 ///< 使用技能的单位结算后的状态副本。
    game::battle::BattleActionResult result{};       ///< 技能行动结算结果。
    std::uint32_t round_index{0};                    ///< 行动开始时所在轮次。
};

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
