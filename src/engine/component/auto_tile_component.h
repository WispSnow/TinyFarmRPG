#pragma once

#include <entt/entity/entity.hpp>

namespace engine::component {

struct AutoTileComponent {
    entt::id_type rule_id_{entt::null};  ///< @brief 自动图块规则ID
    uint8_t current_mask_{0};            ///< @brief 当前 8-bit 邻接掩码（N/NE/E/SE/S/SW/W/NW）
};

} // namespace engine::component