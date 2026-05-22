#pragma once

#include <entt/core/fwd.hpp>
#include <entt/entity/entity.hpp>

#include <string>

namespace game::defs {

struct AdvanceTimeRequest {
    int hours{0};
};

struct ToggleLightRequest {
    entt::id_type light_type_id{entt::null};
};

struct AsyncSaveCompletedEvent {
    std::string file_path{};
    bool success{false};
    std::string error{};
};

} // namespace game::defs
