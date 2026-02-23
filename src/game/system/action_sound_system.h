#pragma once

#include "engine/system/task_event_buffer.h"

#include <entt/entity/fwd.hpp>
#include <entt/signal/fwd.hpp>

namespace game::system {

class ActionSoundSystem {
    entt::registry& registry_;
    entt::dispatcher& dispatcher_;

public:
    ActionSoundSystem(entt::registry& registry, entt::dispatcher& dispatcher);
    void update(float delta_time, engine::system::TaskEventBuffer* task_events = nullptr);
};

} // namespace game::system
