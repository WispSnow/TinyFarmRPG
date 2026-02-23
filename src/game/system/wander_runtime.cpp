#include "game/system/wander_runtime.h"

#include "engine/utils/math.h"
#include "game/component/tags.h"

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>

#include <cmath>
#include <limits>

namespace {

[[nodiscard]] game::component::Direction resolveDirection(const glm::vec2 direction) {
    const auto abs_x = std::abs(direction.x);
    const auto abs_y = std::abs(direction.y);
    if (abs_x > abs_y) {
        return direction.x < 0.0f ? game::component::Direction::Left : game::component::Direction::Right;
    }
    return direction.y < 0.0f ? game::component::Direction::Up : game::component::Direction::Down;
}

void setStateIfChanged(game::component::StateComponent& state,
                       const game::component::Action action,
                       const game::component::Direction direction,
                       const entt::entity entity,
                       engine::system::DeferredCommands& deferred) {
    bool dirty = false;
    if (state.action_ != action) {
        state.action_ = action;
        dirty = true;
    }
    if (state.direction_ != direction) {
        state.direction_ = direction;
        dirty = true;
    }
    if (dirty) {
        deferred.emplaceOrReplace<game::component::StateDirtyTag>(entity);
    }
}

void setIdleIfChanged(game::component::StateComponent& state,
                      const entt::entity entity,
                      engine::system::DeferredCommands& deferred) {
    if (state.action_ != game::component::Action::Idle) {
        state.action_ = game::component::Action::Idle;
        deferred.emplaceOrReplace<game::component::StateDirtyTag>(entity);
    }
}

} // namespace

namespace game::system::wander_runtime {

void stopMovement(game::component::WanderComponent& wander,
                  engine::component::VelocityComponent& velocity) {
    velocity.velocity_ = glm::vec2(0.0f);
    wander.phase_ = game::component::WanderPhase::Waiting;
}

void tickMovement(game::component::WanderComponent& wander,
                  const engine::component::TransformComponent& transform,
                  engine::component::VelocityComponent& velocity,
                  game::component::StateComponent& state,
                  const float speed,
                  const float delta_time,
                  const entt::entity entity,
                  engine::system::DeferredCommands& deferred) {
    if (wander.radius_ <= std::numeric_limits<float>::epsilon()) {
        stopMovement(wander, velocity);
        setIdleIfChanged(state, entity, deferred);
        return;
    }

    if (wander.phase_ == game::component::WanderPhase::Waiting) {
        wander.wait_timer_ -= delta_time;
        velocity.velocity_ = glm::vec2(0.0f);
        setIdleIfChanged(state, entity, deferred);
        if (wander.wait_timer_ > 0.0f) {
            return;
        }

        const float angle = engine::utils::randomFloat(0.0f, glm::two_pi<float>());
        const float radius = engine::utils::randomFloat(0.0f, wander.radius_);
        wander.target_ = wander.home_position_ + glm::vec2(std::cos(angle), std::sin(angle)) * radius;
        wander.phase_ = game::component::WanderPhase::Moving;
        wander.stuck_timer_ = 0.0f;
        const glm::vec2 to_target = wander.target_ - transform.position_;
        wander.last_distance_sq_ = glm::dot(to_target, to_target);
    }

    if (wander.phase_ != game::component::WanderPhase::Moving) {
        return;
    }

    const glm::vec2 to_target = wander.target_ - transform.position_;
    const float distance_sq = glm::dot(to_target, to_target);
    if (distance_sq <= 4.0f) {
        stopMovement(wander, velocity);
        wander.wait_timer_ = engine::utils::randomFloat(wander.min_wait_, wander.max_wait_);
        setIdleIfChanged(state, entity, deferred);
        return;
    }

    const auto direction = glm::normalize(to_target);
    velocity.velocity_ = direction * speed;

    if (distance_sq >= wander.last_distance_sq_ - 1.0f) {
        wander.stuck_timer_ += delta_time;
        if (wander.stuck_timer_ >= wander.stuck_reset_) {
            stopMovement(wander, velocity);
            wander.wait_timer_ = engine::utils::randomFloat(wander.min_wait_, wander.max_wait_);
            setIdleIfChanged(state, entity, deferred);
            return;
        }
    } else {
        wander.stuck_timer_ = 0.0f;
    }
    wander.last_distance_sq_ = distance_sq;

    setStateIfChanged(state,
                      game::component::Action::Walk,
                      resolveDirection(direction),
                      entity,
                      deferred);
}

} // namespace game::system::wander_runtime
