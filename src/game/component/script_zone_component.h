#pragma once

#include "engine/utils/math.h"

#include <entt/entity/entity.hpp>

#include <string>

namespace game::component {

/// @brief Rectangular map zone that emits Lua-facing enter/exit events.
struct ScriptZoneComponent {
    engine::utils::Rect rect_{};
    entt::id_type map_id_{entt::null};
    std::string zone_id_{};
    entt::id_type zone_id_hash_{entt::null};
};

} // namespace game::component
