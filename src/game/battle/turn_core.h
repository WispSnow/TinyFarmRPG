#pragma once

#include "battle_types.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <unordered_map>
#include <vector>

namespace game::battle {

/// @brief 负责回合顺序、轮次推进与胜负判定的纯领域核心。
///
/// TurnCore 持有可变单位列表，并刻意与 UI、ECS、dispatcher、场景栈代码解耦。
/// 构造时按 speed 降序稳定排序，推进时跳过已战斗不能单位，并在每次状态变更后
/// 根据双方存活情况判定胜负。
class TurnCore final {
public:
    /// @brief 当前行动者跨过轮次边界时触发的回调。
    using RoundHook = std::function<void(std::uint32_t round_index)>;

private:
    std::vector<BattleUnit> units_{};                                      ///< 当前战斗的完整可变单位状态。
    std::unordered_map<BattleUnitId, std::size_t> unit_index_by_id_{};      ///< 从战斗单位 ID 到 units_ 下标的查找表。
    std::vector<BattleUnitId> turn_order_{};                                ///< 按 BattleUnit::speed 降序稳定排序得到的行动顺序。
    std::size_t current_turn_index_{0};                                     ///< 当前存活行动者在 turn_order_ 中的下标。
    std::uint32_t round_index_{0};                                          ///< 存在可行动单位后从 1 开始计数的轮次索引。
    BattleOutcome outcome_{BattleOutcome::Ongoing};                         ///< 最近一次判定后的战斗结果缓存。
    std::optional<BattleOutcome> forced_outcome_{};                          ///< Escape 等非 HP 终局覆盖；设置后不再自动重算。
    RoundHook on_round_begin_{};                                            ///< round_index_ 递增后触发的可选回调。
    RoundHook on_round_end_{};                                              ///< 离开已完成轮次前触发的可选回调。

public:
    /// @brief 构造回合核心并生成稳定行动顺序。
    /// @param units 初始单位状态；单位 ID 应在列表内唯一。
    explicit TurnCore(std::vector<BattleUnit> units);

    /// @brief 返回当前可变领域状态的只读视图。
    [[nodiscard]] const std::vector<BattleUnit>& units() const { return units_; }

    /// @brief 返回目标选择和回合推进使用的行动顺序。
    [[nodiscard]] const std::vector<BattleUnitId>& turnOrder() const { return turn_order_; }

    /// @brief 按 ID 查找单位。（调用方不可修改）
    /// @return 找到时返回单位指针，否则返回 nullptr。
    [[nodiscard]] const BattleUnit* findUnit(BattleUnitId id) const;

    /// @brief 按 ID 查找可变单位。（调用方可修改）
    /// @return 找到时返回单位指针，否则返回 nullptr。
    [[nodiscard]] BattleUnit* findUnitMutable(BattleUnitId id);

    /// @brief 返回当前拥有回合的存活行动者。
    /// @return 战斗结束或没有可用存活行动者时返回 nullopt。
    [[nodiscard]] std::optional<BattleUnitId> currentActorId() const;

    /// @brief 返回缓存的战斗结果。
    [[nodiscard]] BattleOutcome outcome() const { return outcome_; }
    [[nodiscard]] bool isBattleEnded() const { return outcome_ != BattleOutcome::Ongoing; }

    /// @brief 返回从 1 开始的轮次索引；首个行动者不存在时为 0。
    [[nodiscard]] std::uint32_t roundIndex() const { return round_index_; }

    /// @brief 安装由会话运行时状态使用的可选轮次切换回调。
    void setRoundHooks(RoundHook on_round_begin, RoundHook on_round_end);

    /// @brief 推进到下一个存活行动者，必要时跨轮。
    /// @return 成功选中下一个存活行动者时返回 true；战斗结束时返回 false。
    [[nodiscard]] bool advanceTurn();

    /// @brief 重新判定胜负，并把当前行动者对齐到存活单位。
    void refresh();

    /// @brief 覆盖缓存结果，用于 Escape 等非 HP 结算。
    /// @param outcome 要强制设置的终局结果。
    void forceOutcome(BattleOutcome outcome);

private:
    /// @brief 根据 units_ 重建 unit_index_by_id_。
    void buildIndex();

    /// @brief 构建按 speed 降序稳定排序的行动顺序。
    void buildTurnOrder();

    /// @brief 根据当前玩家/敌方存活集合更新 outcome_。
    void evaluateOutcome();

    /// @brief 必要时将 current_turn_index_ 移动到下一个存活单位。
    void alignCurrentActor();
    [[nodiscard]] bool isAlive(BattleUnitId id) const;
};

} // namespace game::battle
