#include "dialogue_bubble_controller.h"

#include "dialogue_bubble_view.h"

#include <spdlog/spdlog.h>

namespace game::ui {

DialogueBubbleController::DialogueBubbleController(entt::dispatcher& dispatcher)
    : dispatcher_(dispatcher) {
    dispatcher_.sink<game::defs::DialogueShowEvent>().connect<&DialogueBubbleController::onShow>(this);
    dispatcher_.sink<game::defs::DialogueMoveEvent>().connect<&DialogueBubbleController::onMove>(this);
    dispatcher_.sink<game::defs::DialogueHideEvent>().connect<&DialogueBubbleController::onHide>(this);
}

DialogueBubbleController::~DialogueBubbleController() {
    dispatcher_.sink<game::defs::DialogueShowEvent>().disconnect<&DialogueBubbleController::onShow>(this);
    dispatcher_.sink<game::defs::DialogueMoveEvent>().disconnect<&DialogueBubbleController::onMove>(this);
    dispatcher_.sink<game::defs::DialogueHideEvent>().disconnect<&DialogueBubbleController::onHide>(this);
}

void DialogueBubbleController::registerBubble(std::uint8_t channel,
                                              DialogueBubbleView* view,
                                              glm::vec2 screen_offset) {
    if (!view) {
        spdlog::warn("DialogueBubbleController::registerBubble: channel={} view=nullptr, ignored.", channel);
        slots_.erase(channel);
        return;
    }

    slots_[channel] = BubbleSlot{
        .view = view,
        .screen_offset = screen_offset,
    };
}

void DialogueBubbleController::unregisterBubble(std::uint8_t channel) {
    slots_.erase(channel);
}

void DialogueBubbleController::onShow(const game::defs::DialogueShowEvent& evt) {
    const auto slot_it = slots_.find(evt.channel);
    if (slot_it == slots_.end()) {
        return;
    }

    auto& slot = slot_it->second;
    if (!slot.view) {
        return;
    }

    slot.view->setWorldAnchor(evt.world_position, slot.screen_offset);
    slot.view->setText(formatDialogueText(evt.speaker, evt.text));
    slot.view->setVisible(true);
}

void DialogueBubbleController::onMove(const game::defs::DialogueMoveEvent& evt) {
    const auto slot_it = slots_.find(evt.channel);
    if (slot_it == slots_.end()) {
        return;
    }

    auto& slot = slot_it->second;
    if (!slot.view) {
        return;
    }

    slot.view->setWorldAnchor(evt.world_position, slot.screen_offset);
}

void DialogueBubbleController::onHide(const game::defs::DialogueHideEvent& evt) {
    const auto slot_it = slots_.find(evt.channel);
    if (slot_it == slots_.end()) {
        return;
    }

    auto& slot = slot_it->second;
    if (!slot.view) {
        return;
    }

    slot.view->clearWorldAnchor();
    slot.view->setVisible(false);
}

std::string DialogueBubbleController::formatDialogueText(std::string_view speaker, std::string_view text) {
    std::string output;
    output.reserve(speaker.size() + text.size() + 4);
    if (!speaker.empty()) {
        output.append(speaker);
        output.append(": ");
        output.push_back('\n');
    }

    output.append(text);

    return output;
}

} // namespace game::ui
