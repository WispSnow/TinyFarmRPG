#pragma once

#include <entt/entity/entity.hpp>

namespace game::defs {

struct AppearanceChangedEvent {
    entt::entity target{entt::null};
};

} // namespace game::defs
