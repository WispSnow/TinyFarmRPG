#pragma once

#include <cstddef>
#include <vector>

namespace game::scene {

/// @brief 给定上一回合记住的下标与当前可用条目，返回应当落点的默认下标。
///
/// 行为：
/// - 若 cursor memory 关闭，直接返回 fallback_index。
/// - 若 remembered_index 越界或对应位置 disabled，返回 fallback_index。
/// - 否则返回 remembered_index。
///
/// 适用于：actor command / skill list / item list / target select 的默认光标解析。
[[nodiscard]] inline int resolveCursorMemoryDefaultIndex(int remembered_index,
                                                         const std::vector<bool>& enabled,
                                                         int fallback_index,
                                                         bool cursor_memory_enabled) noexcept {
    if (!cursor_memory_enabled) {
        return fallback_index;
    }
    if (remembered_index < 0 || static_cast<std::size_t>(remembered_index) >= enabled.size()) {
        return fallback_index;
    }
    if (!enabled[static_cast<std::size_t>(remembered_index)]) {
        return fallback_index;
    }
    return remembered_index;
}

} // namespace game::scene
