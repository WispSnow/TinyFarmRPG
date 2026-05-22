#pragma once

#include <entt/core/fwd.hpp>
#include <entt/entity/entity.hpp>

#include <string>

namespace game::defs {

struct QuestOfferRequestedEvent {
    entt::entity player{entt::null};
    entt::entity giver{entt::null};
    entt::id_type quest_id_hash{entt::null};
    std::string quest_id{};
};

struct QuestAcceptedEvent {
    entt::entity player{entt::null};
    entt::entity giver{entt::null};
    entt::id_type quest_id_hash{entt::null};
    std::string quest_id{};
};

struct QuestCompletedEvent {
    entt::entity player{entt::null};
    entt::entity giver{entt::null};
    entt::id_type quest_id_hash{entt::null};
    std::string quest_id{};
};

} // namespace game::defs
