#pragma once
#include <glm/vec2.hpp>

namespace engine::component {

/**
 * @brief 矩形碰撞盒 (适合箱子、庄稼、墙壁等)
 * @note 注意碰撞盒的参照点是矩形正中心。
 */
struct AABBCollider {
    glm::vec2 size_{};
    glm::vec2 offset_{}; // 相对 Sprite 中心的偏移
};

/**
 * @brief 圆形碰撞盒 (适合玩家脚底、圆形投射物等)
 * @note 注意碰撞盒的参照点是圆形正中心。
 */
struct CircleCollider {
    float radius_{};
    glm::vec2 offset_{};
};

} // namespace engine::component