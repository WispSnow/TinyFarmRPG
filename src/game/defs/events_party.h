#pragma once

#include <entt/entity/entity.hpp>

#include <string>

namespace game::defs {

struct PartyRuntimeStatsChanged {
    entt::entity player{entt::null};
    std::string actor_id{};
    bool full_sync{false};
};

} // namespace game::defs
