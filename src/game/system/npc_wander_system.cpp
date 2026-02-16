#include "npc_wander_system.h"
#include "game/component/npc_component.h"
#include "game/component/actor_component.h"
#include "game/component/state_component.h"
#include "game/component/tags.h"
#include "engine/component/transform_component.h"
#include "engine/component/velocity_component.h"
#include "engine/utils/math.h"
#include <entt/entity/registry.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <spdlog/spdlog.h>
#include <limits>

namespace {

game::component::Direction resolveDirection(glm::vec2 direction) {
    const auto abs_x = std::abs(direction.x);
    const auto abs_y = std::abs(direction.y);

    if (abs_x > abs_y) {
        return direction.x < 0.0f ? game::component::Direction::Left : game::component::Direction::Right;
    }
    return direction.y < 0.0f ? game::component::Direction::Up : game::component::Direction::Down;
}

} // namespace

namespace game::system {

NPCWanderSystem::NPCWanderSystem(entt::registry& registry)
    : registry_(registry) {}

void NPCWanderSystem::update(float delta_time) {
    auto view = registry_.view<game::component::WanderComponent,
                               engine::component::TransformComponent,
                               engine::component::VelocityComponent,
                               game::component::ActorComponent,
                               game::component::StateComponent>();

    for (auto entity : view) {
        auto& wander = view.get<game::component::WanderComponent>(entity);
        auto& transform = view.get<engine::component::TransformComponent>(entity);
        auto& velocity = view.get<engine::component::VelocityComponent>(entity);
        auto& actor = view.get<game::component::ActorComponent>(entity);
        auto& state = view.get<game::component::StateComponent>(entity);

        const auto* sleep = registry_.try_get<game::component::SleepRoutine>(entity);
        const bool is_sleeping = sleep && sleep->is_sleeping_;
        const auto* dialogue = registry_.try_get<game::component::DialogueComponent>(entity);
        const bool in_dialogue = dialogue && dialogue->active_;
        const auto* animal_behavior = registry_.try_get<game::component::AnimalBehaviorState>(entity);
        const bool eating = animal_behavior && animal_behavior->eating_;

        // 睡眠优先：保持 Sleep 状态，不要覆盖为 Idle
        if (is_sleeping) {
            velocity.velocity_ = glm::vec2(0.0f);
            wander.moving_ = false;
            wander.has_target_ = false;
            continue;
        }

        if (in_dialogue || eating || wander.radius_ <= std::numeric_limits<float>::epsilon()) {
            velocity.velocity_ = glm::vec2(0.0f);
            if (!eating && state.action_ != game::component::Action::Idle) {
                state.action_ = game::component::Action::Idle;
                registry_.emplace_or_replace<game::component::StateDirtyTag>(entity);
            }
            wander.moving_ = false;
            wander.has_target_ = false;
            continue;
        }

        if (!wander.moving_) {
            wander.wait_timer_ -= delta_time;
            velocity.velocity_ = glm::vec2(0.0f);
            if (state.action_ != game::component::Action::Idle) {
                state.action_ = game::component::Action::Idle;
                registry_.emplace_or_replace<game::component::StateDirtyTag>(entity);
            }
            if (wander.wait_timer_ > 0.0f) {
                continue;
            }
            // 选择新的目标点
            const float angle = engine::utils::randomFloat(0.0f, glm::two_pi<float>());
            const float radius = engine::utils::randomFloat(0.0f, wander.radius_);
            wander.target_ = wander.home_position_ + glm::vec2(std::cos(angle), std::sin(angle)) * radius;
            wander.has_target_ = true;
            wander.moving_ = true;
            wander.stuck_timer_ = 0.0f;
            wander.last_distance_sq_ = glm::dot(wander.target_ - transform.position_, wander.target_ - transform.position_);
        }

        if (wander.moving_ && wander.has_target_) {
            const glm::vec2 to_target = wander.target_ - transform.position_;
            const float distance_sq = glm::dot(to_target, to_target);
            if (distance_sq <= 4.0f) { // 2px 阈值
                wander.moving_ = false;
                wander.has_target_ = false;
                wander.wait_timer_ = engine::utils::randomFloat(wander.min_wait_, wander.max_wait_);
                velocity.velocity_ = glm::vec2(0.0f);
                if (state.action_ != game::component::Action::Idle) {
                    state.action_ = game::component::Action::Idle;
                    registry_.emplace_or_replace<game::component::StateDirtyTag>(entity);
                }
                continue;
            }

            const auto direction = glm::normalize(to_target);
            velocity.velocity_ = direction * actor.speed_;
            auto new_direction = resolveDirection(direction);
            // 卡住检测：如果距离没有减少，累积计时；超过阈值则放弃当前目标并重新等待
            if (distance_sq >= wander.last_distance_sq_ - 1.0f) {
                wander.stuck_timer_ += delta_time;
                if (wander.stuck_timer_ >= wander.stuck_reset_) {
                    wander.moving_ = false;
                    wander.has_target_ = false;
                    wander.wait_timer_ = engine::utils::randomFloat(wander.min_wait_, wander.max_wait_);
                    velocity.velocity_ = glm::vec2(0.0f);
                    continue;
                }
            } else {
                wander.stuck_timer_ = 0.0f;
            }
            wander.last_distance_sq_ = distance_sq;
            bool dirty = false;
            if (state.action_ != game::component::Action::Walk) {
                state.action_ = game::component::Action::Walk;
                dirty = true;
            }
            if (state.direction_ != new_direction) {
                state.direction_ = new_direction;
                dirty = true;
            }
            if (dirty) {
                registry_.emplace_or_replace<game::component::StateDirtyTag>(entity);
            }
        }
    }
}

} // namespace game::system
