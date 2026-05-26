#pragma once

#include "game/battle/battle_types.h"

#include <string>
#include <vector>

namespace game::data {
class ItemCatalog;
class RpgCatalog;
} // namespace game::data

namespace game::runtime {
class LocalizationService;
} // namespace game::runtime

namespace game::battle {

/// @brief 战斗日志行的轻量语义，用于表现层映射颜色。
enum class BattleLogTone {
    Normal,
    Damage,
    Recovery,
    State,
    System,
    Error
};

/// @brief formatter 输出的纯数据日志行。
struct BattleLogLine {
    std::string text{};
    BattleLogTone tone{BattleLogTone::Normal};

    friend bool operator==(const BattleLogLine& lhs, const BattleLogLine& rhs) = default;
};

/// @brief 日志格式化所需的可选目录依赖。
struct BattleLogFormatterContext {
    const game::data::RpgCatalog* rpg_catalog{nullptr};
    const game::data::ItemCatalog* item_catalog{nullptr};
    const game::runtime::LocalizationService* localization{nullptr};
};

/// @brief 将单次行动结果格式化为可显示的短战斗日志行。
[[nodiscard]] std::vector<BattleLogLine> formatBattleLogLines(
    const BattleActionResult& result,
    BattleLogFormatterContext context = {});

} // namespace game::battle
