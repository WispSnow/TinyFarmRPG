#include "rml_screen_fade.h"

#include "rml_ui_runtime.h"

#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <format>

namespace {

constexpr float ALPHA_EPSILON = 1.0e-4f;
constexpr std::string_view DOCUMENT_PATH = "ui/rmlui/overlay/screen_fade.rml";

[[nodiscard]] float clamp01(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

} // namespace

namespace engine::ui::rmlui {

RmlScreenFade::RmlScreenFade(RmlUiRuntime& runtime, uint64_t owner_scene_id)
    : runtime_(runtime) {
    document_ = runtime_.loadDocument(DOCUMENT_PATH, owner_scene_id);
    if (!document_) {
        spdlog::error("RmlScreenFade: failed to load '{}'.", DOCUMENT_PATH);
        return;
    }

    overlay_ = document_->GetElementById("fade-overlay");
    if (!overlay_) {
        spdlog::error("RmlScreenFade: #fade-overlay element not found.");
        runtime_.unloadDocument(document_);
        document_ = nullptr;
        return;
    }

    // Start hidden
    runtime_.hideDocument(document_);
    spdlog::debug("RmlScreenFade initialized.");
}

RmlScreenFade::~RmlScreenFade() {
    if (document_) {
        runtime_.unloadDocument(document_);
        document_ = nullptr;
    }
}

void RmlScreenFade::fadeOut(float seconds) {
    startFade(Phase::FadingOut, 1.0f, seconds);
}

void RmlScreenFade::fadeIn(float seconds) {
    startFade(Phase::FadingIn, 0.0f, seconds);
}

void RmlScreenFade::update(float delta_time) {
    if (phase_ == Phase::Idle) {
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
            applyOpacity();
            return;
        }

        if (phase_ == Phase::FadingIn) {
            phase_ = Phase::Idle;
            alpha_ = 0.0f;
            setOverlayInteractive(false);
            applyOpacity();
            if (document_) {
                runtime_.hideDocument(document_);
            }
            return;
        }
    }

    applyOpacity();
}

void RmlScreenFade::startFade(Phase next_phase, float target_alpha, float seconds) {
    phase_ = next_phase;
    from_alpha_ = clamp01(alpha_);
    to_alpha_ = clamp01(target_alpha);
    duration_ = std::max(0.0f, seconds);
    timer_ = 0.0f;

    if (document_) {
        runtime_.showDocument(document_);
    }
    setOverlayInteractive(true);
    applyOpacity();
}

void RmlScreenFade::applyOpacity() {
    if (!overlay_) {
        return;
    }
    overlay_->SetProperty("opacity", std::format("{:.4f}", clamp01(alpha_)));
}

void RmlScreenFade::setOverlayInteractive(bool interactive) {
    if (!overlay_) {
        return;
    }
    if (interactive) {
        overlay_->SetProperty("pointer-events", "auto");
    } else {
        overlay_->SetProperty("pointer-events", "none");
    }
}

} // namespace engine::ui::rmlui
