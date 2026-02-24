#include "world_anchor_ui_controller.h"

#include "dialogue_bubble_view.h"
#include "engine/core/context.h"
#include "engine/render/camera.h"

namespace game::ui {

WorldAnchorUIController::WorldAnchorUIController(entt::dispatcher& dispatcher,
                                                 engine::core::Context& context)
    : dispatcher_(dispatcher),
      context_(context) {
    dispatcher_.sink<game::defs::DialogueShowEvent>().connect<&WorldAnchorUIController::onDialogueShow>(this);
    dispatcher_.sink<game::defs::DialogueMoveEvent>().connect<&WorldAnchorUIController::onDialogueMove>(this);
    dispatcher_.sink<game::defs::DialogueHideEvent>().connect<&WorldAnchorUIController::onDialogueHide>(this);
}

WorldAnchorUIController::~WorldAnchorUIController() {
    dispatcher_.sink<game::defs::DialogueShowEvent>().disconnect<&WorldAnchorUIController::onDialogueShow>(this);
    dispatcher_.sink<game::defs::DialogueMoveEvent>().disconnect<&WorldAnchorUIController::onDialogueMove>(this);
    dispatcher_.sink<game::defs::DialogueHideEvent>().disconnect<&WorldAnchorUIController::onDialogueHide>(this);
}

void WorldAnchorUIController::registerDialogueBubble(std::uint8_t channel,
                                                     DialogueBubbleView* view,
                                                     glm::vec2 screen_offset) {
    if (!view) {
        dialogue_slots_.erase(channel);
        return;
    }

    auto& slot = dialogue_slots_[channel];
    slot.view = view;
    slot.screen_offset = screen_offset;
    slot.world_position = glm::vec2{0.0f};
    slot.active = false;
    slot.view->setVisible(false);
}

void WorldAnchorUIController::syncProjectedPositions() {
    auto& camera = context_.getCamera();
    for (auto& [_, slot] : dialogue_slots_) {
        if (!slot.active) {
            continue;
        }

        const glm::vec2 screen_pos = camera.worldToScreen(slot.world_position);
        slot.view->setPosition(screen_pos + slot.screen_offset);
    }
}

void WorldAnchorUIController::onDialogueShow(const game::defs::DialogueShowEvent& evt) {
    AnchorSlot* slot = findSlot(evt.channel);
    if (!slot) {
        return;
    }

    slot->world_position = evt.world_position;
    slot->active = true;
    slot->view->setText(formatDialogueText(evt.speaker, evt.text));
    slot->view->setVisible(true);
}

void WorldAnchorUIController::onDialogueMove(const game::defs::DialogueMoveEvent& evt) {
    AnchorSlot* slot = findSlot(evt.channel);
    if (!slot) {
        return;
    }

    slot->world_position = evt.world_position;
}

void WorldAnchorUIController::onDialogueHide(const game::defs::DialogueHideEvent& evt) {
    AnchorSlot* slot = findSlot(evt.channel);
    if (!slot) {
        return;
    }

    slot->active = false;
    slot->view->setVisible(false);
}

WorldAnchorUIController::AnchorSlot* WorldAnchorUIController::findSlot(const std::uint8_t channel) {
    const auto it = dialogue_slots_.find(channel);
    if (it == dialogue_slots_.end()) {
        return nullptr;
    }
    return &it->second;
}

std::string WorldAnchorUIController::formatDialogueText(std::string_view speaker, std::string_view text) {
    constexpr std::size_t MAX_CHARS_PER_LINE = 28;
    std::string output;
    output.reserve(text.size() + speaker.size() + 4);

    if (!speaker.empty()) {
        output.append(speaker);
        output.append(": ");
        output.push_back('\n');
    }

    std::size_t line_len = 0;
    for (const char c : text) {
        output.push_back(c);
        if (c == '\n') {
            line_len = 0;
            continue;
        }

        ++line_len;
        if (line_len >= MAX_CHARS_PER_LINE) {
            // 尝试在空格处换行，否则强制换行。
            std::size_t back = output.size();
            while (back > 0 &&
                   output[back - 1] != ' ' &&
                   output[back - 1] != '\n' &&
                   (output.size() - back) < MAX_CHARS_PER_LINE) {
                --back;
            }

            if (back > 0 && output[back - 1] == ' ') {
                output[back - 1] = '\n';
            } else {
                output.push_back('\n');
            }
            line_len = 0;
        }
    }

    return output;
}

} // namespace game::ui
