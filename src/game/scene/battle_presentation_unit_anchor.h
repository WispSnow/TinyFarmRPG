#pragma once

#include "game/battle/battle_types.h"

#include <glm/vec2.hpp>

namespace game::scene {

/// @brief 战斗表现层单位锚点，记录不受动画 pose 影响的阵型屏幕坐标。
struct BattlePresentationUnitAnchor {
    game::battle::BattleUnitId unit_id{0};                 ///< 战斗单位 ID。
    game::battle::BattleSide side{game::battle::BattleSide::Player}; ///< 单位所属阵营。
    glm::vec2 base_screen_position{0.0f, 0.0f};            ///< 阵型屏幕坐标，不包含攻击 / 受击演出偏移。
    bool alive_after{true};                                ///< 本次结算后是否仍存活。
};

} // namespace game::scene
