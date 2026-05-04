#pragma once

#include <entt/core/fwd.hpp>
#include <entt/entity/entity.hpp>

#include <string>

namespace game::component {

/// @brief 标记一个地图 actor 可通过对话确认加入玩家队伍。
struct RecruitableComponent {
    std::string actor_id_{};
    entt::id_type actor_id_hash_{entt::null};
};

} // namespace game::component
