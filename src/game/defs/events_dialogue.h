#pragma once

#include <entt/core/fwd.hpp>
#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>

#include <cstdint>
#include <string>

namespace game::defs {

enum class DialogueChannel : std::uint8_t {
    Conversation = 0,
    Notice = 1,
    ItemNotice = 2,
};

struct DialogueShowEvent {
    entt::entity target{entt::null};
    std::string speaker{};
    std::string text{};
    glm::vec2 world_position{0.0f};
    DialogueChannel channel{DialogueChannel::Conversation};
    entt::id_type speaker_actor_id_hash{entt::null}; ///< @brief 明确说话人 actor，用于头像解析。
    std::string speaker_actor_id{};                   ///< @brief 可读 actor id，主要用于脚本/调试链路。
};

struct DialogueMoveEvent {
    entt::entity target{entt::null};
    glm::vec2 world_position{0.0f};
    DialogueChannel channel{DialogueChannel::Conversation};
};

struct DialogueHideEvent {
    entt::entity target{entt::null};
    DialogueChannel channel{DialogueChannel::Conversation};
};

} // namespace game::defs
