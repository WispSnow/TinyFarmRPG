#pragma once

#include "game/component/tags.h"
#include "game/defs/events.h"
#include "engine/component/sprite_component.h"
#include "engine/component/transform_component.h"

#include <entt/entity/entity.hpp>
#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>
#include <glm/vec2.hpp>

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>

namespace game::data {

struct PlayerEntityContext {
    entt::entity entity{entt::null};
};

} // namespace game::data

namespace game::system::helpers {

struct NotificationTimer {
    entt::entity target{entt::null};
    float remaining_seconds{0.0f};
};

[[nodiscard]] inline entt::entity getPlayerEntity(entt::registry& registry) {
    auto* cached = registry.ctx().find<game::data::PlayerEntityContext>();
    if (cached && cached->entity != entt::null) {
        if (registry.valid(cached->entity) &&
            registry.all_of<game::component::PlayerTag, engine::component::TransformComponent>(cached->entity)) {
            return cached->entity;
        }
        cached->entity = entt::null;
    }

    auto view = registry.view<game::component::PlayerTag, engine::component::TransformComponent>();
    const entt::entity player = view.begin() == view.end() ? entt::null : *view.begin();

    if (!cached) {
        cached = &registry.ctx().emplace<game::data::PlayerEntityContext>();
    }
    cached->entity = player;
    return player;
}

[[nodiscard]] inline glm::vec2 computeHeadPosition(entt::registry& registry, entt::entity entity) {
    if (auto* transform = registry.try_get<engine::component::TransformComponent>(entity)) {
        glm::vec2 offset{0.0f, 0.0f};
        if (auto* sprite = registry.try_get<engine::component::SpriteComponent>(entity)) {
            offset.y -= sprite->size_.y * sprite->pivot_.y;
        } else {
            offset.y -= 16.0f;
        }
        return transform->position_ + offset;
    }
    return glm::vec2{0.0f};
}

inline void emitDialogueBubbleShow(entt::dispatcher& dispatcher,
                                  std::uint8_t channel,
                                  entt::entity target,
                                  std::string speaker,
                                  std::string text,
                                  glm::vec2 world_position) {
    game::defs::DialogueShowEvent evt{};
    evt.target = target;
    evt.speaker = std::move(speaker);
    evt.text = std::move(text);
    evt.world_position = world_position;
    evt.channel = channel;
    dispatcher.trigger(evt);
}

inline void emitDialogueBubbleMove(entt::dispatcher& dispatcher,
                                  std::uint8_t channel,
                                  entt::entity target,
                                  glm::vec2 world_position) {
    game::defs::DialogueMoveEvent evt{};
    evt.target = target;
    evt.world_position = world_position;
    evt.channel = channel;
    dispatcher.trigger(evt);
}

inline void emitDialogueBubbleHide(entt::dispatcher& dispatcher, std::uint8_t channel, entt::entity target) {
    game::defs::DialogueHideEvent evt{};
    evt.target = target;
    evt.channel = channel;
    dispatcher.enqueue(evt);
}

inline void showTimedNotification(entt::registry& registry,
                                  entt::dispatcher& dispatcher,
                                  std::uint8_t channel,
                                  NotificationTimer& timer,
                                  entt::entity target,
                                  std::string speaker,
                                  std::string text,
                                  float seconds) {
    if (target == entt::null || text.empty() || seconds <= 0.0f) {
        timer = {};
        return;
    }

    timer.target = target;
    timer.remaining_seconds = seconds;
    emitDialogueBubbleShow(
        dispatcher,
        channel,
        target,
        std::move(speaker),
        std::move(text),
        computeHeadPosition(registry, target));
}

inline void updateTimedNotification(entt::registry& registry,
                                    entt::dispatcher& dispatcher,
                                    std::uint8_t channel,
                                    NotificationTimer& timer,
                                    float delta_time) {
    if (timer.target == entt::null || timer.remaining_seconds <= 0.0f) {
        return;
    }

    timer.remaining_seconds = std::max(0.0f, timer.remaining_seconds - delta_time);

    if (registry.valid(timer.target)) {
        emitDialogueBubbleMove(dispatcher, channel, timer.target, computeHeadPosition(registry, timer.target));
    }

    if (timer.remaining_seconds <= 0.0f) {
        emitDialogueBubbleHide(dispatcher, channel, timer.target);
        timer = {};
    }
}

} // namespace game::system::helpers
