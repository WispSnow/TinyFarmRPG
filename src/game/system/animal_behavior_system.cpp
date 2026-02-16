#include "animal_behavior_system.h"
#include "game/component/npc_component.h"
#include "game/component/state_component.h"
#include "game/component/tags.h"
#include "engine/component/velocity_component.h"
#include "engine/component/transform_component.h"
#include "game/data/game_time.h"
#include <glm/vec2.hpp>
#include <entt/entity/registry.hpp>
#include <spdlog/spdlog.h>
#include "engine/utils/math.h"

using namespace entt::literals;

namespace game::system {

AnimalBehaviorSystem::AnimalBehaviorSystem(entt::registry& registry)
    : registry_(registry) {}

void AnimalBehaviorSystem::update(float delta_time) {
    auto* game_time = registry_.ctx().find<game::data::GameTime>();
    if (!game_time) {
        spdlog::warn("AnimalBehaviorSystem: GameTime 未找到");
        return;
    }

    const bool is_night = game_time->time_of_day_ == game::data::TimeOfDay::Night;

    auto view = registry_.view<game::component::SleepRoutine,
                               game::component::StateComponent,
                               game::component::AnimalBehaviorState,
                               game::component::WanderComponent,
                               engine::component::VelocityComponent>();
    for (auto entity : view) {
        auto& sleep = view.get<game::component::SleepRoutine>(entity);
        auto& state = view.get<game::component::StateComponent>(entity);
        auto& behavior = view.get<game::component::AnimalBehaviorState>(entity);
        auto& wander = view.get<game::component::WanderComponent>(entity);
        auto& velocity = view.get<engine::component::VelocityComponent>(entity);

        const bool should_sleep = sleep.sleep_at_night_ && is_night;

        if (should_sleep && !sleep.is_sleeping_) {
            sleep.is_sleeping_ = true;
            velocity.velocity_ = glm::vec2(0.0f);
            wander.moving_ = false;
            wander.has_target_ = false;
            wander.wait_timer_ = 0.0f;
            behavior.eating_ = false;
            behavior.eat_duration_timer_ = 0.0f;
            state.action_ = game::component::Action::Sleep;
            // 目前没有上方向睡觉动画帧，因此用下方向替代。其他方向正常
            if (state.direction_ == game::component::Direction::Up) state.direction_ = game::component::Direction::Down;
            registry_.emplace_or_replace<game::component::StateDirtyTag>(entity);
        } else if (!should_sleep && sleep.is_sleeping_) {
            sleep.is_sleeping_ = false;
            state.action_ = game::component::Action::Idle;
            registry_.emplace_or_replace<game::component::StateDirtyTag>(entity);
        }

        if (sleep.is_sleeping_) {
            continue;
        }

        // 进食状态维护
        if (behavior.eating_) {
            behavior.eat_duration_timer_ -= delta_time;
            velocity.velocity_ = glm::vec2(0.0f);
            wander.moving_ = false;
            wander.has_target_ = false;
            wander.wait_timer_ = 0.0f;
            if (state.action_ != game::component::Action::Eat) {
                state.action_ = game::component::Action::Eat;
                if (state.direction_ == game::component::Direction::Up) {
                    state.direction_ = game::component::Direction::Down;
                }
                registry_.emplace_or_replace<game::component::StateDirtyTag>(entity);
            }
            if (behavior.eat_duration_timer_ <= 0.0f) {
                behavior.eating_ = false;
                behavior.eat_cooldown_timer_ = engine::utils::randomFloat(behavior.eat_interval_min_, behavior.eat_interval_max_);
                state.action_ = game::component::Action::Idle;
                registry_.emplace_or_replace<game::component::StateDirtyTag>(entity);
            }
            continue;
        }

        // 触发进食
        behavior.eat_cooldown_timer_ -= delta_time;
        if (behavior.eat_cooldown_timer_ <= 0.0f) {
            behavior.eating_ = true;
            behavior.eat_duration_timer_ = behavior.eat_duration_;
            velocity.velocity_ = glm::vec2(0.0f);
            wander.moving_ = false;
            wander.has_target_ = false;
            wander.wait_timer_ = 0.0f;
            state.action_ = game::component::Action::Eat;
            if (state.direction_ == game::component::Direction::Up) {
                state.direction_ = game::component::Direction::Down;
            }
            registry_.emplace_or_replace<game::component::StateDirtyTag>(entity);
        }
    }
}

} // namespace game::system
