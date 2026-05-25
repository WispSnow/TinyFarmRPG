#include "game/scene/battle_input_router.h"

#include "engine/input/input_manager.h"

#include <entt/core/hashed_string.hpp>

namespace {

using namespace entt::literals;

constexpr float REPEAT_INITIAL_DELAY_SECONDS = 0.28f;
constexpr float REPEAT_INTERVAL_SECONDS = 0.08f;

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
    clearRepeat();
}

void BattleInputRouter::update(const float delta_time) {
    if (!connected_ || !input_manager_ || !delegate_ || repeat_direction_ == RepeatDirection::None) {
        return;
    }

    if (delegate_->battleMenuState() == BattleMenuState::None || !repeatDirectionStillDown()) {
        clearRepeat();
        return;
    }

    repeat_timer_seconds_ -= delta_time;
    while (repeat_timer_seconds_ <= 0.0f && repeat_direction_ != RepeatDirection::None) {
        if (!dispatchRepeatDirection()) {
            repeat_timer_seconds_ = REPEAT_INTERVAL_SECONDS;
            return;
        }
        repeat_timer_seconds_ += REPEAT_INTERVAL_SECONDS;
    }
}

void BattleInputRouter::clearRepeat() {
    repeat_direction_ = RepeatDirection::None;
    repeat_timer_seconds_ = 0.0f;
}

bool BattleInputRouter::onMenuUpPressed() {
    beginRepeat(RepeatDirection::Up);
    return moveVertical(-1);
}

bool BattleInputRouter::onMenuDownPressed() {
    beginRepeat(RepeatDirection::Down);
    return moveVertical(1);
}

bool BattleInputRouter::onMenuLeftPressed() {
    beginRepeat(RepeatDirection::Left);
    return moveHorizontal(-1);
}

bool BattleInputRouter::onMenuRightPressed() {
    beginRepeat(RepeatDirection::Right);
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
    const bool moved = delegate_->moveBattleMenuCursor(direction);
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

bool BattleInputRouter::repeatDirectionStillDown() const {
    if (!input_manager_) {
        return false;
    }

    switch (repeat_direction_) {
        case RepeatDirection::Up:
            return input_manager_->isActionDown("menu_up"_hs);
        case RepeatDirection::Down:
            return input_manager_->isActionDown("menu_down"_hs);
        case RepeatDirection::Left:
            return input_manager_->isActionDown("menu_left"_hs);
        case RepeatDirection::Right:
            return input_manager_->isActionDown("menu_right"_hs);
        case RepeatDirection::None:
            return false;
    }
    return false;
}

bool BattleInputRouter::dispatchRepeatDirection() {
    switch (repeat_direction_) {
        case RepeatDirection::Up:
            return moveVertical(-1);
        case RepeatDirection::Down:
            return moveVertical(1);
        case RepeatDirection::Left:
            return moveHorizontal(-1);
        case RepeatDirection::Right:
            return moveHorizontal(1);
        case RepeatDirection::None:
            return false;
    }
    return false;
}

void BattleInputRouter::beginRepeat(const RepeatDirection direction) {
    repeat_direction_ = direction;
    repeat_timer_seconds_ = REPEAT_INITIAL_DELAY_SECONDS;
}

} // namespace game::scene
