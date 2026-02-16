#pragma once

#include <string>
#include "engine/utils/math.h"
#include <entt/core/hashed_string.hpp>
#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>

namespace game::component {

struct MapId {
    entt::id_type id_{entt::null};
};

/// 落点方向偏移（玩家从哪个方向进入目标地图）
enum class StartOffset {
    None,
    Left,
    Right,
    Top,
    Bottom
};

struct MapTrigger {
    engine::utils::Rect rect_{};          ///< 触发器区域（世界坐标）
    entt::id_type map_id_{entt::null};    ///< 当前地图ID
    int self_id_{0};                      ///< 当前地图触发器ID
    int target_id_{0};                    ///< 目标地图触发器ID
    entt::id_type target_map_{entt::null};///< 目标地图ID
    std::string target_map_name_{};       ///< 目标地图名称（无扩展名）
    StartOffset start_offset_{StartOffset::None}; ///< 落点方向偏移
};

struct RestArea {
    engine::utils::Rect rect_{};  ///< 休息区域范围（世界坐标）
};

} // namespace game::component
