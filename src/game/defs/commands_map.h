#pragma once

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>

#include <string>

namespace game::defs {

/// @brief 请求把玩家传送到指定地图坐标。
///
/// MapTransitionSystem 负责实际加载地图、应用 fade、校正安全落点并发布 map_enter/map_exit 事件。
struct WarpToMapCommand {
    entt::entity player{entt::null};
    std::string map_id{};
    glm::vec2 position{0.0F, 0.0F};
};

} // namespace game::defs
