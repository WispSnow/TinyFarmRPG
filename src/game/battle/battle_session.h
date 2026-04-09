#pragma once

#include "battle_action_resolver.h"
#include "battle_runtime_types.h"
#include "battle_types.h"
#include "turn_core.h"

#include <entt/core/fwd.hpp>

#include <optional>
#include <unordered_map>
#include <vector>

namespace game::data {
class RpgCatalog;
class ItemCatalog;
} // namespace game::data

namespace game::battle {

/// @brief 创建 BattleSession 时注入的可选外部数据。
struct BattleSessionOptions {
    const game::data::RpgCatalog* rpg_catalog{nullptr};       ///< RPG 数据目录，用于查找技能、状态等战斗数据。
    const game::data::ItemCatalog* item_catalog{nullptr};     ///< 物品数据目录，用于查找可在战斗中使用的道具。
    std::unordered_map<entt::id_type, int> item_stocks{};     ///< 进入战斗时复制出的道具库存，键为哈希后的物品 ID。
};

/// @brief 表现层与 TurnCore 之间的应用层门面。
///
/// BattleSession 是 BattleScene 进入领域逻辑的唯一入口：它接收 BattleAction，
/// 委托 BattleActionResolver 完成校验与效果应用，并在每次提交后生成全量
/// BattleSnapshot 供 UI 刷新或测试断言使用。
class BattleSession final {
    TurnCore turn_core_;                         ///< 纯领域回合核心，负责行动顺序与胜负判定。
    BattleActionResolver resolver_{};            ///< 动作结算器，负责具体行动校验、公式和目录数据应用。
    BattleRuntimeState runtime_state_{};         ///< 战斗会话内的临时运行时状态。

public:
    /// @brief 构造战斗会话。
    /// @param units 初始战斗单位；通常由命令或 BattleUnit 工厂提供。
    /// @param options 技能/物品目录与库存等可选依赖。
    explicit BattleSession(std::vector<BattleUnit> units, BattleSessionOptions options = {});
    [[nodiscard]] const std::vector<BattleUnit>& units() const { return turn_core_.units(); }
    [[nodiscard]] const BattleUnit* findUnit(BattleUnitId id) const { return turn_core_.findUnit(id); }

    /// @brief 返回当前行动者；战斗结束时为 nullopt。
    [[nodiscard]] std::optional<BattleUnitId> currentActorId() const { return turn_core_.currentActorId(); }
    [[nodiscard]] BattleOutcome outcome() const { return turn_core_.outcome(); }

    /// @brief 返回战斗内剩余道具库存；该库存是进入战斗时复制出的运行时副本。
    [[nodiscard]] const std::unordered_map<entt::id_type, int>& itemStocks() const { return runtime_state_.item_stocks; }

    /// @brief 生成当前完整战斗快照。
    [[nodiscard]] BattleSnapshot snapshot() const;

    /// @brief 提交并处理一个战斗行动。
    /// @param action 玩家、AI 或脚本产生的行动意图。
    /// @return 包含动作结果与全量快照的处理结果。
    [[nodiscard]] BattleActionResult submitAction(const BattleAction& action);

private:
    /// @brief 将最新快照和结果写回 BattleActionResult。
    void fillSnapshot(BattleActionResult& result) const;
};

} // namespace game::battle
