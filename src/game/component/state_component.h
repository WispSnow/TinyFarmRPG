#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace game::component {

/**
 * @brief 游戏动作分类
 */
enum class Action : std::uint8_t {
    Idle = 0,
    Walk,
    Hoe,
    Watering,
    Planting,
    Sickle,
    Pickaxe,
    Axe,
    Sleep,
    Eat,
    Count
};

/**
 * @brief 朝向分类
 */
enum class Direction : std::uint8_t {
    Up = 0,
    Down,
    Left,
    Right,
    Count
};

namespace detail {

inline constexpr std::array<std::string_view, static_cast<std::size_t>(Action::Count)> ACTION_NAMES{
    "idle",
    "walk",
    "hoe",
    "watering",
    "planting",
    "sickle",
    "pickaxe",
    "axe",
    "sleep",
    "eat"
};

inline constexpr std::array<std::string_view, static_cast<std::size_t>(Direction::Count)> DIRECTION_NAMES{
    "up",
    "down",
    "left",
    "right"
};

} // namespace detail

/**
 * @brief 辅助转换函数
 */
[[nodiscard]] inline constexpr std::string_view actionToString(Action action) {
    const auto idx = static_cast<std::size_t>(action);
    if (idx >= detail::ACTION_NAMES.size()) return "unknown";
    return detail::ACTION_NAMES[idx];
}

[[nodiscard]] inline constexpr std::string_view directionToString(Direction direction) {
    const auto idx = static_cast<std::size_t>(direction);
    if (idx >= detail::DIRECTION_NAMES.size()) return "unknown";
    return detail::DIRECTION_NAMES[idx];
}

/**
 * @brief 角色状态组件，用于驱动动画状态机
 */
struct StateComponent {
    Action action_{Action::Idle};
    Direction direction_{Direction::Down};
};

} // namespace game::component
