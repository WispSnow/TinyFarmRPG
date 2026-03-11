#include "engine/ui/ui_navigation_controller.h"

#include "engine/input/input_manager.h"

#include <entt/core/hashed_string.hpp>

using namespace entt::literals;

namespace engine::ui {

UINavigationController::UINavigationController(engine::input::InputManager& input_manager)
    : input_manager_(&input_manager) {
    input_manager_->onAction("menu_up"_hs).connect<&UINavigationController::handleMenuUpPressed>(this);
    input_manager_->onAction("menu_down"_hs).connect<&UINavigationController::handleMenuDownPressed>(this);
    input_manager_->onAction("menu_left"_hs).connect<&UINavigationController::handleMenuLeftPressed>(this);
    input_manager_->onAction("menu_right"_hs).connect<&UINavigationController::handleMenuRightPressed>(this);
    input_manager_->onAction("menu_confirm"_hs).connect<&UINavigationController::handleMenuConfirmPressed>(this);
}

UINavigationController::~UINavigationController() {
    disconnect();
}

entt::sink<entt::sigh<void()>> UINavigationController::onNavigateUp() {
    return navigate_up_signal_;
}

entt::sink<entt::sigh<void()>> UINavigationController::onNavigateDown() {
    return navigate_down_signal_;
}

entt::sink<entt::sigh<void()>> UINavigationController::onNavigateLeft() {
    return navigate_left_signal_;
}

entt::sink<entt::sigh<void()>> UINavigationController::onNavigateRight() {
    return navigate_right_signal_;
}

entt::sink<entt::sigh<void()>> UINavigationController::onConfirm() {
    return confirm_signal_;
}

void UINavigationController::disconnect() {
    if (!input_manager_) {
        return;
    }

    input_manager_->onAction("menu_up"_hs).disconnect<&UINavigationController::handleMenuUpPressed>(this);
    input_manager_->onAction("menu_down"_hs).disconnect<&UINavigationController::handleMenuDownPressed>(this);
    input_manager_->onAction("menu_left"_hs).disconnect<&UINavigationController::handleMenuLeftPressed>(this);
    input_manager_->onAction("menu_right"_hs).disconnect<&UINavigationController::handleMenuRightPressed>(this);
    input_manager_->onAction("menu_confirm"_hs).disconnect<&UINavigationController::handleMenuConfirmPressed>(this);
    input_manager_ = nullptr;
}

bool UINavigationController::handleMenuUpPressed() {
    navigate_up_signal_.publish();
    return false;
}

bool UINavigationController::handleMenuDownPressed() {
    navigate_down_signal_.publish();
    return false;
}

bool UINavigationController::handleMenuLeftPressed() {
    navigate_left_signal_.publish();
    return false;
}

bool UINavigationController::handleMenuRightPressed() {
    navigate_right_signal_.publish();
    return false;
}

bool UINavigationController::handleMenuConfirmPressed() {
    confirm_signal_.publish();
    return false;
}

} // namespace engine::ui
