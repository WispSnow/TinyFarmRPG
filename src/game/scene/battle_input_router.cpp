#include "game/scene/battle_input_router.h"

#include "engine/input/input_manager.h"

#include <entt/core/hashed_string.hpp>

namespace {

using namespace entt::literals;

constexpr int PARTY_COMMAND_COLUMNS = 1;
constexpr int ACTOR_COMMAND_COLUMNS = 2;

[[nodiscard]] int columnStepFor(const game::scene::BattleMenuState state) {
    switch (state) {
        case game::scene::BattleMenuState::ActorCommand:
            return ACTOR_COMMAND_COLUMNS;
        case game::scene::BattleMenuState::PartyCommand:
            return PARTY_COMMAND_COLUMNS;
        case game::scene::BattleMenuState::SkillList:
        case game::scene::BattleMenuState::ItemList:
        case game::scene::BattleMenuState::TargetSelect:
        case game::scene::BattleMenuState::None:
            return 1;
    }
    return 1;
}

} // namespace

namespace game::scene {

BattleInputRouter::~BattleInputRouter() {
    disconnect();
}

void BattleInputRouter::connect(engine::input::InputManager& input_manager, Delegate& delegate) {
    if (connected_) {
        return;
    }

    input_manager_ = &input_manager;
    delegate_ = &delegate;
    input_manager.onAction("menu_up"_hs).connect<&BattleInputRouter::onMenuUpPressed>(this);
    input_manager.onAction("menu_down"_hs).connect<&BattleInputRouter::onMenuDownPressed>(this);
    input_manager.onAction("menu_left"_hs).connect<&BattleInputRouter::onMenuLeftPressed>(this);
    input_manager.onAction("menu_right"_hs).connect<&BattleInputRouter::onMenuRightPressed>(this);
    input_manager.onAction("menu_confirm"_hs).connect<&BattleInputRouter::onMenuConfirmPressed>(this);
    input_manager.onAction("menu_cancel"_hs).connect<&BattleInputRouter::onMenuCancelPressed>(this);
    connected_ = true;
}

void BattleInputRouter::disconnect() {
    if (!connected_ || input_manager_ == nullptr) {
        return;
    }

    input_manager_->onAction("menu_up"_hs).disconnect<&BattleInputRouter::onMenuUpPressed>(this);
    input_manager_->onAction("menu_down"_hs).disconnect<&BattleInputRouter::onMenuDownPressed>(this);
    input_manager_->onAction("menu_left"_hs).disconnect<&BattleInputRouter::onMenuLeftPressed>(this);
    input_manager_->onAction("menu_right"_hs).disconnect<&BattleInputRouter::onMenuRightPressed>(this);
    input_manager_->onAction("menu_confirm"_hs).disconnect<&BattleInputRouter::onMenuConfirmPressed>(this);
    input_manager_->onAction("menu_cancel"_hs).disconnect<&BattleInputRouter::onMenuCancelPressed>(this);
    input_manager_ = nullptr;
    delegate_ = nullptr;
    connected_ = false;
}

bool BattleInputRouter::onMenuUpPressed() {
    return moveVertical(-1);
}

bool BattleInputRouter::onMenuDownPressed() {
    return moveVertical(1);
}

bool BattleInputRouter::onMenuLeftPressed() {
    return moveHorizontal(-1);
}

bool BattleInputRouter::onMenuRightPressed() {
    return moveHorizontal(1);
}

bool BattleInputRouter::onMenuConfirmPressed() {
    return delegate_ ? delegate_->confirmBattleMenu() : false;
}

bool BattleInputRouter::onMenuCancelPressed() {
    return delegate_ ? delegate_->cancelBattleMenu() : false;
}

bool BattleInputRouter::moveVertical(const int direction) {
    if (!delegate_) {
        return false;
    }

    const BattleMenuState menu_state = delegate_->battleMenuState();
    const bool moved = delegate_->moveBattleMenuCursor(direction * columnStepFor(menu_state));
    return moved || menu_state != BattleMenuState::None;
}

bool BattleInputRouter::moveHorizontal(const int direction) {
    if (!delegate_) {
        return false;
    }

    const BattleMenuState menu_state = delegate_->battleMenuState();
    const bool moved = delegate_->moveBattleMenuCursor(direction);
    return moved || menu_state != BattleMenuState::None;
}

} // namespace game::scene
