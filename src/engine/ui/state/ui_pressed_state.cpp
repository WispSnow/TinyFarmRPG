#include "ui_pressed_state.h"
#include "ui_normal_state.h"
#include "ui_hover_state.h"
#include "engine/ui/ui_interactive.h"
#include <spdlog/spdlog.h>
#include <entt/core/hashed_string.hpp>

using namespace entt::literals;

namespace engine::ui::state {

void UIPressedState::enter()
{
    owner_->applyStateVisual(UI_IMAGE_PRESSED_ID);
    owner_->playSoundEvent(UI_SOUND_EVENT_CLICK_ID);
    spdlog::trace("切换到按下状态");
}

void UIPressedState::onMouseReleased(bool is_inside)
{
    if (is_inside) {
        owner_->setNextState(std::make_unique<UIHoverState>(owner_));
        owner_->clicked();
    } else {
        owner_->setNextState(std::make_unique<UINormalState>(owner_));
    }
}

} // namespace engine::ui::state
