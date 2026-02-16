#include "ui_hover_state.h"
#include "ui_normal_state.h"
#include "ui_pressed_state.h"
#include "engine/ui/ui_interactive.h"
#include <spdlog/spdlog.h>
#include <entt/core/hashed_string.hpp>

using namespace entt::literals;

namespace engine::ui::state {

void UIHoverState::enter()
{
    owner_->applyStateVisual(UI_IMAGE_HOVER_ID);
    owner_->hover_enter();
    spdlog::trace("切换到悬停状态");
}

void UIHoverState::onMouseExit()
{
    owner_->hover_leave();
    owner_->setNextState(std::make_unique<UINormalState>(owner_));
}

void UIHoverState::onMousePressed()
{
    owner_->setNextState(std::make_unique<UIPressedState>(owner_));
}

} // namespace engine::ui::state
