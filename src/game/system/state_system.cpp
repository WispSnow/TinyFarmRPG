#include "state_system.h"
#include "game/component/tags.h"
#include "engine/component/tags.h"
#include "engine/component/animation_component.h"
#include "engine/utils/events.h"
#include <entt/core/hashed_string.hpp>
#include <entt/entity/registry.hpp>
#include <spdlog/spdlog.h>
#include <string>
#include <algorithm>
#include <array>
#include <vector>
#include <format>
#include <iterator>

namespace {

/// 需要循环播放的动作（待机/行走/睡觉），其余动作单次播放
constexpr std::array LOOPING_ACTIONS = {
    game::component::Action::Idle,
    game::component::Action::Walk,
    game::component::Action::Sleep,
};

[[nodiscard]] bool isLoopingAction(game::component::Action action) {
    return std::ranges::find(LOOPING_ACTIONS, action) != LOOPING_ACTIONS.end();
}

[[nodiscard]] std::string buildAnimationKey(game::component::Action action,
                                            game::component::Direction direction) {
    return std::format("{}_{}",
                       game::component::actionToString(action),
                       game::component::directionToString(direction));
}

} // namespace

namespace game::system {

StateSystem::StateSystem(entt::registry& registry, entt::dispatcher& dispatcher)
    : registry_(registry), dispatcher_(dispatcher) {
    buildAnimationLookup();
    fallback_animation_id_ = resolveAnimationId(game::component::Action::Idle, game::component::Direction::Down, nullptr);
    dispatcher_.sink<::engine::utils::AnimationFinishedEvent>().connect<&StateSystem::onAnimationFinishedEvent>(this);
}

StateSystem::~StateSystem() {
    dispatcher_.disconnect(this);
}

void StateSystem::update() {
    auto view = registry_.view<game::component::StateComponent, game::component::StateDirtyTag>(entt::exclude<::engine::component::NeedRemoveTag>);
    std::vector<entt::entity> dirty_entities;
    dirty_entities.reserve(view.size_hint());
    std::ranges::copy(view, std::back_inserter(dirty_entities));

    for (auto entity : dirty_entities) {
        const auto& state = registry_.get<game::component::StateComponent>(entity);
        const auto* anim_component = registry_.try_get<::engine::component::AnimationComponent>(entity);
        auto animation_id = resolveAnimationId(state.action_, state.direction_, anim_component);
        const bool loop = isLoopingAction(state.action_);
        dispatcher_.enqueue(::engine::utils::PlayAnimationEvent{entity, animation_id, loop});
        spdlog::trace("播放动画: {}, {}", entt::to_integral(animation_id), loop);
        registry_.remove<game::component::StateDirtyTag>(entity);
    }
}

void StateSystem::buildAnimationLookup() {
    for (std::size_t action_index = 0; action_index < ACTION_COUNT; ++action_index) {
        for (std::size_t direction_index = 0; direction_index < DIRECTION_COUNT; ++direction_index) {
            const auto action = static_cast<game::component::Action>(action_index);
            const auto direction = static_cast<game::component::Direction>(direction_index);
            const auto key = buildAnimationKey(action, direction);
            animation_lookup_[action_index][direction_index] = entt::hashed_string(key.c_str());
        }
    }
}

entt::id_type StateSystem::resolveAnimationId(game::component::Action action,
                                                          game::component::Direction direction,
                                                          const ::engine::component::AnimationComponent* anim_component) const {
    const auto action_index = static_cast<std::size_t>(action);
    const auto direction_index = static_cast<std::size_t>(direction);
    const auto primary = animation_lookup_[action_index][direction_index];
    if (anim_component && anim_component->animations_.contains(primary)) {
        return primary;
    }
    if (direction == game::component::Direction::Left) {
        const auto right_id = animation_lookup_[action_index][static_cast<std::size_t>(game::component::Direction::Right)];
        if (anim_component && anim_component->animations_.contains(right_id)) {
            return right_id;
        }
    }
    if (anim_component && anim_component->animations_.contains(fallback_animation_id_)) {
        return fallback_animation_id_;
    }
    return primary;
}

void StateSystem::onAnimationFinishedEvent(const ::engine::utils::AnimationFinishedEvent& event) {
    registry_.remove<game::component::ActionLockedTag>(event.entity_);
    if (auto state = registry_.try_get<game::component::StateComponent>(event.entity_); state) {
        state->action_ = game::component::Action::Idle;
        registry_.emplace_or_replace<game::component::StateDirtyTag>(event.entity_);
    }   
}

} // namespace game::system
