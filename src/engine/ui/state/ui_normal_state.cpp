#include "ui_normal_state.h"
#include "engine/ui/ui_interactive.h"

namespace engine::ui::state {

void UINormalState::enter()
{}

void UINormalState::onMouseEnter()
{
    owner_->transitionTo(InteractionPhase::Hovered);
}

void UINormalState::onMousePressed()
{
    owner_->transitionTo(InteractionPhase::Pressed);
}

} // namespace engine::ui::state
