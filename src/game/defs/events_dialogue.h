#pragma once

#include <entt/core/fwd.hpp>
#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>

#include <cstdint>
#include <string>
#include <vector>

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

struct DialogueChoiceOption {
    std::string id{};
    std::string label{};
};

/// @brief 请求打开一组剧情选项；UI 选择完成后发 DialogueChoiceSelectedEvent。
struct DialogueChoiceRequestedEvent {
    std::uint32_t request_id{0};
    entt::entity target{entt::null};
    std::string prompt{};
    std::string speaker{};
    std::string speaker_actor_id{};
    entt::id_type speaker_actor_id_hash{entt::null};
    std::vector<DialogueChoiceOption> options{};
    bool allow_cancel{true};
};

/// @brief 剧情选项 UI 的结果。option_index 为 0-based；cancelled=true 时为 -1。
struct DialogueChoiceSelectedEvent {
    std::uint32_t request_id{0};
    entt::entity target{entt::null};
    int option_index{-1};
    std::string choice_id{};
    std::string choice_label{};
    bool cancelled{false};
};

} // namespace game::defs
