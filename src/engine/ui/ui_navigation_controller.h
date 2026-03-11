#pragma once

#include <entt/signal/fwd.hpp>
#include <entt/signal/sigh.hpp>

namespace engine::input {
class InputManager;
}

namespace engine::ui {

class UINavigationController final {
public:
    explicit UINavigationController(engine::input::InputManager& input_manager);
    ~UINavigationController();

    UINavigationController(const UINavigationController&) = delete;
    UINavigationController& operator=(const UINavigationController&) = delete;
    UINavigationController(UINavigationController&&) = delete;
    UINavigationController& operator=(UINavigationController&&) = delete;

    entt::sink<entt::sigh<void()>> onNavigateUp();
    entt::sink<entt::sigh<void()>> onNavigateDown();
    entt::sink<entt::sigh<void()>> onNavigateLeft();
    entt::sink<entt::sigh<void()>> onNavigateRight();
    entt::sink<entt::sigh<void()>> onConfirm();

    void disconnect();

private:
    bool handleMenuUpPressed();
    bool handleMenuDownPressed();
    bool handleMenuLeftPressed();
    bool handleMenuRightPressed();
    bool handleMenuConfirmPressed();

    engine::input::InputManager* input_manager_{nullptr};
    entt::sigh<void()> navigate_up_signal_{};
    entt::sigh<void()> navigate_down_signal_{};
    entt::sigh<void()> navigate_left_signal_{};
    entt::sigh<void()> navigate_right_signal_{};
    entt::sigh<void()> confirm_signal_{};
};

} // namespace engine::ui
