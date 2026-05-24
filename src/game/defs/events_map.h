#pragma once

#include <entt/entity/entity.hpp>

#include <string>

namespace game::defs {

struct MapEnteredEvent {
    entt::id_type map_id{entt::null};
    std::string map_name{};
    entt::id_type previous_map_id{entt::null};
    std::string previous_map_name{};
};

struct MapExitedEvent {
    entt::id_type map_id{entt::null};
    std::string map_name{};
    entt::id_type next_map_id{entt::null};
    std::string next_map_name{};
};

} // namespace game::defs
