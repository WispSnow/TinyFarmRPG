#pragma once

#include "ui_interactive.h"

namespace engine::ui {

class UIScreenFade final : public UIInteractive {
public:
    enum class Phase {
        Idle,
        FadingOut,
        Holding,
        FadingIn
    };
private:
    Phase phase_{Phase::Idle};
    float alpha_{0.0f};
    float from_alpha_{0.0f};
    float to_alpha_{0.0f};
    float duration_{0.0f};
    float timer_{0.0f};

public:
    explicit UIScreenFade(engine::core::Context& context);

    void fadeOut(float seconds);
    void fadeIn(float seconds);

    [[nodiscard]] bool isBusy() const { return phase_ != Phase::Idle; }
    [[nodiscard]] bool isFullyOpaque() const;
    [[nodiscard]] bool isFullyTransparent() const;

    [[nodiscard]] float alpha() const { return alpha_; }
    [[nodiscard]] Phase phase() const { return phase_; }

    void update(float delta_time, engine::core::Context& context) override;

protected:
    void renderSelf(engine::core::Context& context) override;
    void startFade(Phase next_phase, float target_alpha, float seconds);
};

} // namespace engine::ui
