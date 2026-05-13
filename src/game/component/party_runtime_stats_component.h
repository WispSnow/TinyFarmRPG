#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

namespace game::component {

struct ActorRuntimeState {
    int current_hp{0};
    int current_mp{0};
};

/// @brief 玩家队伍战斗外运行时状态。挂在玩家实体上，按 actor id 持久化。
struct PartyRuntimeStatsComponent {
    std::unordered_map<std::string, ActorRuntimeState> states_by_actor_id_{};
    std::uint32_t revision_{0};
};

} // namespace game::component
