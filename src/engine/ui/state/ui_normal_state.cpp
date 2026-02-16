#include "ui_normal_state.h"
#include "ui_hover_state.h"
#include "engine/ui/ui_interactive.h"
#include <spdlog/spdlog.h>
#include <entt/core/hashed_string.hpp>

using namespace entt::literals;

namespace engine::ui::state {

void UINormalState::enter()
{
    owner_->applyStateVisual(UI_IMAGE_NORMAL_ID);
    spdlog::trace("切换到正常状态");
}

void UINormalState::onMouseEnter()
{
    owner_->playSoundEvent(UI_SOUND_EVENT_HOVER_ID);
    owner_->setNextState(std::make_unique<UIHoverState>(owner_));
}

} // namespace engine::ui::state
