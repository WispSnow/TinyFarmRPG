#pragma once

#include <cctype>
#include <string_view>

namespace game::data {

inline constexpr std::string_view DEFAULT_BATTLE_BACKGROUND_ID = "Grassland";

/// @brief 校验可拼接到 BattleBg/battlebacks{1,2}/<id>.png 的背景逻辑 id。
///
/// 当前只允许 `[A-Za-z0-9_]+`，优先防止路径穿越和目录分隔符进入资源路径。
/// 引入外部 RPG Maker 资源包时，如果需要支持空格或连字符，应先重新评估路径安全策略。
[[nodiscard]] inline bool isValidBattleBackgroundId(const std::string_view id) {
    if (id.empty()) {
        return false;
    }

    for (const char ch : id) {
        const auto value = static_cast<unsigned char>(ch);
        if (std::isalnum(value) == 0 && ch != '_') {
            return false;
        }
    }
    return true;
}

} // namespace game::data
