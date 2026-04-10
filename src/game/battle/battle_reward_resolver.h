#pragma once

#include "game/battle/battle_types.h"

#include <entt/core/fwd.hpp>

#include <functional>
#include <random>
#include <string>
#include <vector>

namespace game::data {
class RpgCatalog;
}

namespace game::battle {

/// @brief 单个物品掉落的聚合结果。
struct BattleRewardItemDrop {
    std::string item_id{};          ///< 原始 catalog string id，供后续反馈文本查表使用。
    entt::id_type item_id_hash{};   ///< 供库存写回直接使用的稳定 hash。
    int count{0};                   ///< 聚合后的掉落数量。
};

/// @brief 单场战斗奖励的汇总结果。
struct BattleRewardSummary {
    int gold_total{0};                               ///< 本场战斗汇总得到的金币。
    int exp_total{0};                                ///< 本场战斗汇总得到的经验值。
    std::vector<BattleRewardItemDrop> item_drops{}; ///< 聚合后的物品掉落列表。

    /// @brief 判断奖励摘要是否为空。
    [[nodiscard]] bool empty() const {
        return gold_total == 0 && exp_total == 0 && item_drops.empty();
    }
};

/// @brief 根据战斗结束状态汇总金币、经验和掉落的纯逻辑 helper。
class BattleRewardResolver final {
public:
    /// @brief 掉落判定使用的随机回调，返回 [0, 1) 区间浮点值。
    using DropRollFn = std::function<float()>;

    BattleRewardResolver() = default;
    explicit BattleRewardResolver(DropRollFn drop_roll_override);

    /// @brief 基于战斗结算快照汇总奖励。
    [[nodiscard]] BattleRewardSummary resolve(BattleOutcome outcome,
                                              const std::vector<BattleUnit>& final_units,
                                              const game::data::RpgCatalog& rpg_catalog);

private:
    std::mt19937 random_engine_{std::random_device{}()};
    DropRollFn drop_roll_override_{};

    /// @brief 生成一次 [0, 1) 掉落判定随机值。
    [[nodiscard]] float nextDropRoll();
};

} // namespace game::battle
