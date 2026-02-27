#pragma once

#include "battle_types.h"

#include <entt/core/fwd.hpp>

#include <string>
#include <unordered_map>

namespace game::battle {

// Runtime-only temporary state for one battle.
// Keep this out of battle_types.h so command/event payloads stay compact.
struct BattleRuntimeState {
    struct UnitRuntimeState {
        bool guarding{false};
        std::unordered_map<std::string, int> state_turns_left{};
    };

    std::uint32_t escape_attempt_count{0};
    std::unordered_map<entt::id_type, int> item_stocks{};
    std::unordered_map<BattleUnitId, UnitRuntimeState> units{};
};

} // namespace game::battle
