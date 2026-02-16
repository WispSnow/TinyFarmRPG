#pragma once

#include <entt/entity/entity.hpp>
#include <entt/core/hashed_string.hpp>

namespace game::component {

enum class ResourceNodeType {
    Unknown = 0,
    Tree,
    Rock
    // TODO: 未来可添加其他资源类型
};

struct ResourceNodeComponent {
    ResourceNodeType type_{ResourceNodeType::Unknown};
    int hits_to_break_{0};
    int hit_count_{0};
    entt::id_type drop_item_id_{entt::null};
    int drop_min_{1};
    int drop_max_{1};
    entt::id_type hit_animation_id_{entt::null};  ///< @brief 可选：命中时播放的动画ID
};

} // namespace game::component
