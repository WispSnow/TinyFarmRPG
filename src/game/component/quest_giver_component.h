#pragma once

#include <string>

#include <entt/core/fwd.hpp>

namespace game::component {

struct QuestGiverComponent {
    std::string quest_id_{};
    entt::id_type quest_id_hash_{entt::null};
};

} // namespace game::component
