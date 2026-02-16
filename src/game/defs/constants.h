#pragma once
#include "engine/utils/math.h"

namespace game::defs {

constexpr float ACTOR_COLLIDER_RADIUS = 5; ///< @brief 角色脚底碰撞器半径

constexpr float TILE_SIZE = 16.0f; ///< @brief 地图格子大小(像素)

constexpr int TOOL_TARGET_TILE_RANGE = 1; ///< @brief 工具/种子可作用的目标范围(格)，以玩家所在格为中心（含对角）

constexpr float TOOL_COOLDOWN = 1.0f; ///< @brief 工具冷却时间(秒)

/// @brief 作物种植偏移量，相对于网格点参照点（坐标在格子中心偏下一点，而非左上角）
constexpr glm::vec2 CROP_OFFSET = glm::vec2(defs::TILE_SIZE * 0.5, defs::TILE_SIZE * 0.6);

constexpr engine::utils::FColor CURSOR_COLOR = engine::utils::FColor(0.0f, 1.0f, 0.0f, 0.2f);  // 绿色，透明度0.2

enum class Tool {
    Hoe,
    WateringCan,
    Pickaxe,
    Axe,
    Sickle,
    None
};

} // namespace game::defs
