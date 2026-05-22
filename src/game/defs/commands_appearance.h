#pragma once

#include <entt/entity/entity.hpp>

#include <string>

namespace game::defs {

struct SetAppearanceSlotCommand {
    entt::entity target{entt::null};
    std::string slot{};
    std::string variant{};
};

struct RefreshAppearanceCommand {
    entt::entity target{entt::null};
};

} // namespace game::defs
