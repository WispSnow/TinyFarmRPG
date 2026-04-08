#pragma once

#include "game/battle/battle_types.h"

#include <string>
#include <vector>

namespace game::data {
class RpgCatalog;
}

namespace game::battle {

/// @brief 从 RPG 目录构建战斗单位时使用的筛选参数。
///
/// 工厂输入刻意独立于 defs::EnterBattleCommand，避免 battle/data 装配辅助代码
/// 反向耦合到全局命令/事件定义。
struct BattleUnitBuildOptions {
    std::vector<std::string> actor_ids{};    ///< 指定要加入玩家方的 actor id；为空时使用目录中的全部 actor。
    std::string troop_id{};                  ///< 指定敌方 troop id；为空时选择第一个有成员的 troop。
};

/// @brief 根据 RPG 目录中的 actor/class/enemy/troop 数据构建 BattleUnit 列表。
/// @param catalog 已加载并通过引用校验的 RPG 目录。
/// @param options 玩家 actor 与敌方 troop 的选择参数。
/// @param out_units 成功时写入玩家单位和敌方单位，玩家单位排在前面。
/// @param out_error 失败时写入人类可读错误信息，成功时清空。
/// @return 成功构建至少一个玩家单位和至少一个敌方单位时返回 true。
[[nodiscard]] bool buildBattleUnitsFromCatalog(const game::data::RpgCatalog& catalog,
                                               const BattleUnitBuildOptions& options,
                                               std::vector<BattleUnit>& out_units,
                                               std::string& out_error);

} // namespace game::battle
