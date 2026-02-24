#pragma once

#include "game/defs/events.h"

#include <entt/signal/dispatcher.hpp>
#include <glm/vec2.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>

namespace engine::core {
    class Context;
}

namespace game::ui {

class DialogueBubbleView;

class WorldAnchorUIController final {
public:
    WorldAnchorUIController(entt::dispatcher& dispatcher, engine::core::Context& context);
    ~WorldAnchorUIController();

    void registerDialogueBubble(std::uint8_t channel,
                                DialogueBubbleView* view,
                                glm::vec2 screen_offset = {0.0f, -4.0f});
    void update();

private:
    struct AnchorSlot {
        DialogueBubbleView* view{nullptr};
        glm::vec2 world_position{0.0f};
        glm::vec2 screen_offset{0.0f, -4.0f};
        bool active{false};
    };

    entt::dispatcher& dispatcher_;
    engine::core::Context& context_;
    std::unordered_map<std::uint8_t, AnchorSlot> dialogue_slots_{};

    void onDialogueShow(const game::defs::DialogueShowEvent& evt);
    void onDialogueMove(const game::defs::DialogueMoveEvent& evt);
    void onDialogueHide(const game::defs::DialogueHideEvent& evt);

    [[nodiscard]] AnchorSlot* findSlot(std::uint8_t channel);
    [[nodiscard]] static std::string formatDialogueText(std::string_view speaker, std::string_view text);
};

} // namespace game::ui

