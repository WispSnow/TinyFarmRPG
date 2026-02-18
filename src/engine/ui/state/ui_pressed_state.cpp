#include "ui_pressed_state.h"
#include "engine/ui/ui_interactive.h"

namespace engine::ui::state {

void UIPressedState::enter()
{}

void UIPressedState::onMouseReleased(bool is_inside)
{
    if (is_inside) {
        owner_->transitionTo(InteractionPhase::Hovered);
        owner_->clicked();
    } else {
        owner_->transitionTo(InteractionPhase::Normal);
    }
}

} // namespace engine::ui::state
