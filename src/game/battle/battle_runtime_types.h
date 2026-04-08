#pragma once

#include "battle_types.h"

#include <entt/core/fwd.hpp>

#include <string>
#include <unordered_map>

namespace game::battle {

/// @brief 单个 BattleSession 拥有的运行时可变状态。
///
/// 这些状态刻意不放入 battle_types.h，避免命令/事件负载膨胀，也避免可序列化
/// 快照暴露道具库存、防御标记等临时记账数据。
struct BattleRuntimeState {
    /// @brief 单个单位的临时标记与状态剩余回合计数。
    struct UnitRuntimeState {
        bool guarding{false};                                        ///< Guard 行动后为 true，直到下一轮开始时清除。
        std::unordered_map<std::string, int> state_turns_left{};      ///< 按状态 ID 记录剩余回合数；过期后移除。
    };

    std::uint32_t escape_attempt_count{0};                              ///< 本场战斗已尝试 Escape 的次数。
    std::unordered_map<entt::id_type, int> item_stocks{};               ///< 战斗内消耗品库存，键为哈希后的物品 ID。
    std::unordered_map<BattleUnitId, UnitRuntimeState> units{};         ///< 以 BattleUnitId 为键的单位运行时状态。
};

} // namespace game::battle
