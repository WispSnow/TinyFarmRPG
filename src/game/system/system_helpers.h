#pragma once

#include "game/component/tags.h"
#include "game/defs/events.h"
#include "engine/component/sprite_component.h"
#include "engine/component/transform_component.h"

#include <entt/entity/entity.hpp>
#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>
#include <glm/vec2.hpp>

#include <cstdint>
#include <string>
#include <utility>

namespace game::data {

struct PlayerEntityContext {
    entt::entity entity{entt::null};
};

} // namespace game::data

namespace game::system::helpers {

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
    dispatcher.enqueue(evt);
}

inline void emitDialogueBubbleMove(entt::dispatcher& dispatcher,
                                  std::uint8_t channel,
                                  entt::entity target,
                                  glm::vec2 world_position) {
    game::defs::DialogueMoveEvent evt{};
    evt.target = target;
    evt.world_position = world_position;
    evt.channel = channel;
    dispatcher.enqueue(evt);
}

inline void emitDialogueBubbleHide(entt::dispatcher& dispatcher, std::uint8_t channel, entt::entity target) {
    game::defs::DialogueHideEvent evt{};
    evt.target = target;
    evt.channel = channel;
    dispatcher.enqueue(evt);
}

} // namespace game::system::helpers

