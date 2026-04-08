#pragma once

#include "game/battle/battle_types.h"
#include "game/data/rpg_data.h"

#include <sol/sol.hpp>

#include <string>
#include <string_view>

namespace game::battle {

/// @brief 使用 Lua 计算 RPG 战斗公式的轻量求值器。
///
/// 求值时会把施法者/行动者绑定为表 `a`，目标绑定为表 `b`，并暴露 RPG 常见
/// 属性名：hp/mp/mhp/mmp/atk/def/mat/mdf/agi/luk。公式必须返回有限数值，
/// 返回值会截断到 int 范围内。
class BattleFormulaEvaluator final {
public:
    /// @brief 构造求值器并打开公式需要的 Lua 基础库。
    BattleFormulaEvaluator();

    /// @brief 计算字符串形式的战斗公式。
    /// @param formula Lua 表达式，例如 `a.atk - b.def / 2`。
    /// @param source 行动来源，会绑定为 Lua 表 `a`。
    /// @param target 行动目标，会绑定为 Lua 表 `b`。
    /// @param out_value 成功时写入公式结果。
    /// @param out_error 失败时写入错误信息，成功时清空。
    /// @return 公式成功加载、执行并返回有限数值时返回 true。
    [[nodiscard]] bool evaluate(std::string_view formula,
                                const BattleUnit& source,
                                const BattleUnit& target,
                                int& out_value,
                                std::string& out_error);

    /// @brief 计算 DamageFormulaData 中保存的公式。
    /// @param damage_formula RPG 目录中的伤害公式数据。
    /// @param source 行动来源，会绑定为 Lua 表 `a`。
    /// @param target 行动目标，会绑定为 Lua 表 `b`。
    /// @param out_value 成功时写入公式结果。
    /// @param out_error 失败时写入错误信息，成功时清空。
    /// @return 公式成功加载、执行并返回有限数值时返回 true。
    [[nodiscard]] bool evaluate(const game::data::DamageFormulaData& damage_formula,
                                const BattleUnit& source,
                                const BattleUnit& target,
                                int& out_value,
                                std::string& out_error);

private:
    sol::state lua_{};    ///< 单个求值器拥有的 Lua 状态；每次求值前刷新 `a` 与 `b` 表。
};

} // namespace game::battle
