#pragma once

#include "interaction_behavior.h"

#include <functional>

namespace engine::ui {

class ClickBehavior final : public InteractionBehavior {
public:
    using ClickCallback = std::function<void(UIInteractive&)>;

    ClickBehavior() = default;
    ~ClickBehavior() override = default;

    void setOnClick(ClickCallback cb) { on_click_ = std::move(cb); }

    void onClick(UIInteractive& owner) override {
        if (on_click_) {
            on_click_(owner);
        }
    }

private:
    ClickCallback on_click_{};
};

} // namespace engine::ui
