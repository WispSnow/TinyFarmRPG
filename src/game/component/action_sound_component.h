#pragma once

#include "state_component.h"
#include <entt/core/fwd.hpp>
#include <unordered_map>

namespace game::component {

struct ActionSoundTriggerConfig {
    float probability_{1.0f};
    float cooldown_seconds_{0.0f};
};

struct ActionSoundComponent {
    Action last_action_{Action::Idle};
    std::unordered_map<entt::id_type, ActionSoundTriggerConfig> triggers_{};
    std::unordered_map<entt::id_type, float> cooldown_remaining_seconds_{};
};

} // namespace game::component

