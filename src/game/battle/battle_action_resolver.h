#pragma once

#include "game/battle/battle_formula_evaluator.h"
#include "game/battle/battle_runtime_types.h"
#include "game/battle/battle_types.h"

#include <cstdint>
#include <functional>
#include <random>
#include <string>
#include <vector>

namespace game::data {
class RpgCatalog;
class ItemCatalog;
struct SkillData;
enum class Scope : std::uint8_t;
} // namespace game::data

namespace game::battle {

class TurnCore;

/// @brief 负责校验并应用单个 BattleAction 的动作结算器。
///
/// Resolver 只处理战斗规则：当前回合归属校验、目标合法性、技能/道具目录查表、
/// 伤害/回复公式、状态效果、防御与逃跑。它不生成 UI 快照，也不直接依赖场景或
/// 事件系统；BattleSession 会在调用后补齐 BattleActionResult::snapshot。
class BattleActionResolver final {
public:
    /// @brief 逃跑与命中等百分比判定使用的随机数回调，便于测试注入。
    using EscapeRollFunc = std::function<int()>;

    /// @brief 动作结算所需的只读目录依赖。
    struct Dependencies {
        const game::data::RpgCatalog* rpg_catalog{nullptr};       ///< 技能、状态等 RPG 目录；Skill 行动需要它。
        const game::data::ItemCatalog* item_catalog{nullptr};     ///< 战斗道具查表所需物品目录；Item 行动需要它。
    };

private:
    Dependencies dependencies_{};                                      ///< 外部只读目录依赖。
    BattleFormulaEvaluator formula_evaluator_{};                       ///< Lua 公式求值器，用于普通攻击与技能伤害/回复公式。
    std::mt19937 random_engine_{std::random_device{}()};               ///< 默认百分比随机源。
    EscapeRollFunc escape_roll_override_{};                            ///< 测试或脚本可注入的随机判定覆盖。

public:
    /// @brief 使用默认随机源与空目录依赖构造结算器。
    BattleActionResolver() = default;

    /// @brief 使用指定随机判定回调构造结算器。
    /// @param escape_roll_override 返回 1..100 百分比投掷值的回调。
    explicit BattleActionResolver(EscapeRollFunc escape_roll_override);

    /// @brief 使用目录依赖和可选随机判定回调构造结算器。
    /// @param dependencies 技能/物品目录依赖。
    /// @param escape_roll_override 返回 1..100 百分比投掷值的可选回调。
    explicit BattleActionResolver(Dependencies dependencies, EscapeRollFunc escape_roll_override = {});

    /// @brief 校验并应用一个战斗行动。
    /// @param action 待处理的行动意图。
    /// @param turn_core 要读取并修改的回合核心。
    /// @param runtime_state 会话内临时状态，例如防御标记、状态计数和道具库存。
    /// @return 动作结算结果；此处不填充全量 snapshot。
    [[nodiscard]] BattleActionResult resolve(const BattleAction& action,
                                             TurnCore& turn_core,
                                             BattleRuntimeState& runtime_state);

private:
    /// @brief 生成一次 1..100 的百分比随机值。
    [[nodiscard]] int nextPercentRoll();

    /// @brief 按技能范围收集合法目标。
    /// @param action 原始行动，用于读取显式 target_id。
    /// @param actor 当前行动者。
    /// @param skill 技能目录数据。
    /// @param turn_core 当前回合核心。
    /// @param out_targets 收集到的可变目标列表。
    /// @param out_error 失败原因。
    /// @return 找到至少一个合法目标时返回 true。
    [[nodiscard]] bool collectSkillTargets(const BattleAction& action,
                                           const BattleUnit& actor,
                                           const game::data::SkillData& skill,
                                           TurnCore& turn_core,
                                           std::vector<BattleUnit*>& out_targets,
                                           std::string& out_error);

    /// @brief 应用技能的附加效果，例如回复、添加状态或移除状态。
    /// @param skill 技能目录数据。
    /// @param target 当前目标单位。
    /// @param target_state 目标的运行时状态。
    /// @param result 要累加写入的动作结果。
    void applySkillEffects(const game::data::SkillData& skill,
                           BattleUnit& target,
                           BattleRuntimeState::UnitRuntimeState& target_state,
                           BattleActionResult& result);

    /// @brief 生成一次逃跑判定用的百分比随机值。
    [[nodiscard]] int nextEscapeRoll();
};

} // namespace game::battle
