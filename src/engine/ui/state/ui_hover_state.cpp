#include "ui_hover_state.h"
#include "engine/ui/ui_interactive.h"

namespace engine::ui::state {

void UIHoverState::enter()
{}

void UIHoverState::onMouseExit()
{
    owner_->transitionTo(InteractionPhase::Normal);
}

void UIHoverState::onMousePressed()
{
    owner_->transitionTo(InteractionPhase::Pressed);
}

} // namespace engine::ui::state
