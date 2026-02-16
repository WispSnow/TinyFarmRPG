#pragma once

#include "interaction_behavior.h"
#include <functional>

namespace engine::ui {

class HoverBehavior final : public InteractionBehavior {
public:
    using HoverCallback = std::function<void(UIInteractive&)>;
    
private:
    HoverCallback on_enter_{};
    HoverCallback on_exit_{};
public:
    void setOnEnter(HoverCallback cb) { on_enter_ = std::move(cb); }
    void setOnExit(HoverCallback cb) { on_exit_ = std::move(cb); }

    void onHoverEnter(UIInteractive& owner) override {
        if (on_enter_) on_enter_(owner);
    }

    void onHoverExit(UIInteractive& owner) override {
        if (on_exit_) on_exit_(owner);
    }
};

} // namespace engine::ui

