#pragma once

#include <entt/core/fwd.hpp>
#include <entt/entity/entity.hpp>

#include <string>

namespace game::defs {

struct RecruitPartyMemberCommand {
    entt::entity player{entt::null};
    entt::entity recruiter{entt::null};
    entt::id_type actor_id_hash{entt::null};
    std::string actor_id{};
};

} // namespace game::defs
