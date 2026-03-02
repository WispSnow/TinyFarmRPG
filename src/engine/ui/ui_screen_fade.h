#pragma once

#include "ui_interactive.h"
#include "engine/ui/screen_fade_interface.h"

namespace engine::ui {

class UIScreenFade final : public UIInteractive, public IScreenFade {
public:
    // Phase 定义继承自 IScreenFade::Phase
    using IScreenFade::Phase;

private:
    Phase phase_{Phase::Idle};
    float alpha_{0.0f};
    float from_alpha_{0.0f};
    float to_alpha_{0.0f};
    float duration_{0.0f};
    float timer_{0.0f};

public:
    explicit UIScreenFade(engine::core::Context& context);

    void fadeOut(float seconds) override;
    void fadeIn(float seconds) override;

    [[nodiscard]] bool isBusy() const override { return phase_ != Phase::Idle; }
    [[nodiscard]] bool isFullyOpaque() const;
    [[nodiscard]] bool isFullyTransparent() const;

    [[nodiscard]] float alpha() const { return alpha_; }
    [[nodiscard]] Phase phase() const override { return phase_; }

    void update(float delta_time, engine::core::Context& context) override;

protected:
    void renderSelf(engine::core::Context& context) override;
    void startFade(Phase next_phase, float target_alpha, float seconds);
};

} // namespace engine::ui
