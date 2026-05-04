#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace game::component {

/// @brief 玩家长期队伍状态，保存已招募与当前参战角色。
struct PartyComponent {
    std::vector<std::string> recruited_actor_ids_{"actor.player"};
    std::vector<std::string> active_actor_ids_{"actor.player"};
    std::size_t max_active_members_{4};
};

} // namespace game::component
