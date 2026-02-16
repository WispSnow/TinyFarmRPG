#include "ui_screen_fade.h"

#include "engine/core/context.h"
#include "engine/render/renderer.h"
#include "engine/utils/defs.h"
#include <algorithm>

namespace engine::ui {

namespace {

constexpr float ALPHA_EPSILON = 1.0e-4f;

[[nodiscard]] float clamp01(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

} // namespace

UIScreenFade::UIScreenFade(engine::core::Context& context)
    : UIInteractive(context, glm::vec2{0.0f, 0.0f}, glm::vec2{0.0f, 0.0f}) {
    setAnchor(glm::vec2{0.0f, 0.0f}, glm::vec2{1.0f, 1.0f});
    setPivot(glm::vec2{0.0f, 0.0f});
    setPosition(glm::vec2{0.0f, 0.0f});
    setVisible(false);
    setInteractive(false);
}

void UIScreenFade::fadeOut(float seconds) {
    startFade(Phase::FadingOut, 1.0f, seconds);
}

void UIScreenFade::fadeIn(float seconds) {
    startFade(Phase::FadingIn, 0.0f, seconds);
}

bool UIScreenFade::isFullyOpaque() const {
    return alpha_ >= 1.0f - ALPHA_EPSILON;
}

bool UIScreenFade::isFullyTransparent() const {
    return alpha_ <= ALPHA_EPSILON;
}

void UIScreenFade::startFade(Phase next_phase, float target_alpha, float seconds) {
    phase_ = next_phase;
    from_alpha_ = clamp01(alpha_);
    to_alpha_ = clamp01(target_alpha);
    duration_ = std::max(0.0f, seconds);
    timer_ = 0.0f;
    setVisible(true);
    setInteractive(true);
}

void UIScreenFade::update(float delta_time, engine::core::Context& context) {
    UIInteractive::update(delta_time, context);

    if (phase_ == Phase::Idle) {
        alpha_ = 0.0f;
        setVisible(false);
        setInteractive(false);
        return;
    }

    if (phase_ == Phase::Holding) {
        alpha_ = 1.0f;
        return;
    }

    timer_ += std::max(0.0f, delta_time);
    float t = 1.0f;
    if (duration_ > 0.0f) {
        t = clamp01(timer_ / duration_);
    }

    alpha_ = clamp01(from_alpha_ + (to_alpha_ - from_alpha_) * t);

    if (t >= 1.0f - ALPHA_EPSILON) {
        alpha_ = to_alpha_;
        if (phase_ == Phase::FadingOut) {
            phase_ = Phase::Holding;
            return;
        }

        if (phase_ == Phase::FadingIn) {
            phase_ = Phase::Idle;
            alpha_ = 0.0f;
            setVisible(false);
            setInteractive(false);
        }
    }
}

void UIScreenFade::renderSelf(engine::core::Context& context) {
    if (alpha_ <= 0.0f) {
        return;
    }

    engine::utils::ColorOptions params{};
    params.start_color = engine::utils::FColor{0.0f, 0.0f, 0.0f, clamp01(alpha_)};
    context.getRenderer().drawUIFilledRect(getBounds(), &params);
}

} // namespace engine::ui
