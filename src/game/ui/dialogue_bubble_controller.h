#pragma once

#include "game/defs/events.h"
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <entt/signal/dispatcher.hpp>
#include <glm/vec2.hpp>

namespace game::ui {

class DialogueBubbleView;

class DialogueBubbleController final {
public:
    explicit DialogueBubbleController(entt::dispatcher& dispatcher);
    ~DialogueBubbleController();

    void registerBubble(std::uint8_t channel,
                        DialogueBubbleView* view,
                        glm::vec2 screen_offset = {0.0f, -4.0f});

    DialogueBubbleController(const DialogueBubbleController&) = delete;
    DialogueBubbleController& operator=(const DialogueBubbleController&) = delete;
    DialogueBubbleController(DialogueBubbleController&&) = delete;
    DialogueBubbleController& operator=(DialogueBubbleController&&) = delete;

private:
    struct BubbleSlot {
        DialogueBubbleView* view{nullptr};
        glm::vec2 screen_offset{0.0f, -4.0f};
    };

    entt::dispatcher& dispatcher_;
    std::unordered_map<std::uint8_t, BubbleSlot> slots_{};

    void onShow(const game::defs::DialogueShowEvent& evt);
    void onMove(const game::defs::DialogueMoveEvent& evt);
    void onHide(const game::defs::DialogueHideEvent& evt);

    static std::string formatDialogueText(std::string_view speaker, std::string_view text);
};

} // namespace game::ui
