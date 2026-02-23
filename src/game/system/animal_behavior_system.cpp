#include "animal_behavior_system.h"

#include "engine/component/transform_component.h"
#include "engine/component/velocity_component.h"
#include "engine/utils/math.h"
#include "game/component/actor_component.h"
#include "game/component/npc_component.h"
#include "game/component/state_component.h"
#include "game/component/tags.h"
#include "game/data/game_time.h"
#include "game/system/wander_runtime.h"

#include <entt/entity/registry.hpp>
#include <glm/vec2.hpp>
#include <spdlog/spdlog.h>

namespace game::system {

AnimalBehaviorSystem::AnimalBehaviorSystem(entt::registry& registry)
    : registry_(registry) {}

void AnimalBehaviorSystem::update(const float delta_time, engine::system::DeferredCommands& deferred) {
    const auto* game_time = registry_.ctx().find<game::data::GameTime>();
    if (!game_time) {
        spdlog::warn("AnimalBehaviorSystem: GameTime 未找到");
        return;
    }

    const bool is_night = game_time->time_of_day_ == game::data::TimeOfDay::Night;

    auto view = registry_.view<game::component::AnimalTag,
                               game::component::SleepRoutine,
                               game::component::StateComponent,
                               game::component::AnimalBehaviorState,
                               game::component::WanderComponent,
                               game::component::ActorComponent,
                               engine::component::TransformComponent,
                               engine::component::VelocityComponent>();
    for (const auto entity : view) {
        auto& sleep = view.get<game::component::SleepRoutine>(entity);
        auto& state = view.get<game::component::StateComponent>(entity);
        auto& behavior = view.get<game::component::AnimalBehaviorState>(entity);
        auto& wander = view.get<game::component::WanderComponent>(entity);
        auto& actor = view.get<game::component::ActorComponent>(entity);
        auto& transform = view.get<engine::component::TransformComponent>(entity);
        auto& velocity = view.get<engine::component::VelocityComponent>(entity);

        const bool should_sleep = sleep.sleep_at_night_ && is_night;
        if (should_sleep) {
            sleep.is_sleeping_ = true;
            behavior.phase_ = game::component::AnimalBehaviorPhase::Wander;
            behavior.eat_duration_timer_ = 0.0f;
            wander_runtime::stopMovement(wander, velocity);
            wander.wait_timer_ = 0.0f;

            game::component::Direction direction = state.direction_;
            if (direction == game::component::Direction::Up) {
                direction = game::component::Direction::Down;
            }
            if (state.action_ != game::component::Action::Sleep || state.direction_ != direction) {
                state.action_ = game::component::Action::Sleep;
                state.direction_ = direction;
                deferred.emplaceOrReplace<game::component::StateDirtyTag>(entity);
            }
            continue;
        }

        if (sleep.is_sleeping_) {
            sleep.is_sleeping_ = false;
            if (state.action_ != game::component::Action::Idle) {
                state.action_ = game::component::Action::Idle;
                deferred.emplaceOrReplace<game::component::StateDirtyTag>(entity);
            }
        }

        if (behavior.phase_ == game::component::AnimalBehaviorPhase::Eating) {
            behavior.eat_duration_timer_ -= delta_time;
            wander_runtime::stopMovement(wander, velocity);
            wander.wait_timer_ = 0.0f;

            game::component::Direction direction = state.direction_;
            if (direction == game::component::Direction::Up) {
                direction = game::component::Direction::Down;
            }
            if (state.action_ != game::component::Action::Eat || state.direction_ != direction) {
                state.action_ = game::component::Action::Eat;
                state.direction_ = direction;
                deferred.emplaceOrReplace<game::component::StateDirtyTag>(entity);
            }

            if (behavior.eat_duration_timer_ <= 0.0f) {
                behavior.phase_ = game::component::AnimalBehaviorPhase::Wander;
                behavior.eat_cooldown_timer_ = engine::utils::randomFloat(behavior.eat_interval_min_, behavior.eat_interval_max_);
                if (state.action_ != game::component::Action::Idle) {
                    state.action_ = game::component::Action::Idle;
                    deferred.emplaceOrReplace<game::component::StateDirtyTag>(entity);
                }
            }
            continue;
        }

        behavior.eat_cooldown_timer_ -= delta_time;
        if (behavior.eat_cooldown_timer_ <= 0.0f) {
            behavior.phase_ = game::component::AnimalBehaviorPhase::Eating;
            behavior.eat_duration_timer_ = behavior.eat_duration_;
            wander_runtime::stopMovement(wander, velocity);
            wander.wait_timer_ = 0.0f;

            game::component::Direction direction = state.direction_;
            if (direction == game::component::Direction::Up) {
                direction = game::component::Direction::Down;
            }
            if (state.action_ != game::component::Action::Eat || state.direction_ != direction) {
                state.action_ = game::component::Action::Eat;
                state.direction_ = direction;
                deferred.emplaceOrReplace<game::component::StateDirtyTag>(entity);
            }
            continue;
        }

        behavior.phase_ = game::component::AnimalBehaviorPhase::Wander;
        wander_runtime::tickMovement(wander,
                                     transform,
                                     velocity,
                                     state,
                                     actor.speed_,
                                     delta_time,
                                     entity,
                                     deferred);
    }
}

} // namespace game::system
