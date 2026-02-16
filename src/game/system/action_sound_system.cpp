#include "action_sound_system.h"

#include "game/component/action_sound_component.h"
#include "game/component/state_component.h"
#include "engine/component/audio_component.h"
#include "engine/component/tags.h"
#include "engine/component/transform_component.h"
#include "engine/utils/events.h"
#include "engine/utils/math.h"

#include <algorithm>
#include <entt/core/hashed_string.hpp>
#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>

namespace game::system {

ActionSoundSystem::ActionSoundSystem(entt::registry& registry, entt::dispatcher& dispatcher)
    : registry_(registry), dispatcher_(dispatcher) {}

void ActionSoundSystem::update(float delta_time) {
    auto view = registry_.view<game::component::ActionSoundComponent,
                               game::component::StateComponent,
                               engine::component::AudioComponent,
                               engine::component::TransformComponent>(
        entt::exclude<engine::component::NeedRemoveTag>
    );

    for (auto entity : view) {
        auto& action_sounds = view.get<game::component::ActionSoundComponent>(entity);
        const auto& state = view.get<game::component::StateComponent>(entity);
        const auto& audio = view.get<engine::component::AudioComponent>(entity);

        for (auto it = action_sounds.cooldown_remaining_seconds_.begin(); it != action_sounds.cooldown_remaining_seconds_.end();) {
            it->second = std::max(0.0f, it->second - delta_time);
            if (it->second <= 0.0f) {
                it = action_sounds.cooldown_remaining_seconds_.erase(it);
            } else {
                ++it;
            }
        }

        if (state.action_ == action_sounds.last_action_) {
            continue;
        }

        action_sounds.last_action_ = state.action_;

        const entt::id_type trigger_id = entt::hashed_string{game::component::actionToString(state.action_).data()};
        const auto cfg_it = action_sounds.triggers_.find(trigger_id);
        if (cfg_it == action_sounds.triggers_.end()) {
            continue;
        }
        if (!audio.sounds_.contains(trigger_id)) {
            continue;
        }
        if (action_sounds.cooldown_remaining_seconds_.contains(trigger_id)) {
            continue;
        }

        const auto& cfg = cfg_it->second;
        if (cfg.probability_ < 1.0f) {
            const float roll = engine::utils::randomFloat(0.0f, 1.0f);
            if (roll > cfg.probability_) {
                continue;
            }
        }

        dispatcher_.enqueue(engine::utils::PlaySoundEvent{entity, trigger_id});
        if (cfg.cooldown_seconds_ > 0.0f) {
            action_sounds.cooldown_remaining_seconds_[trigger_id] = cfg.cooldown_seconds_;
        }
    }
}

} // namespace game::system

