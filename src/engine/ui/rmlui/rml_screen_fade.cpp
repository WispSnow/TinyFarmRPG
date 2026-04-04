#include "rml_screen_fade.h"

#include "rml_ui_runtime.h"

#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Event.h>
#include <spdlog/spdlog.h>

#include <format>

namespace {

constexpr std::string_view DOCUMENT_PATH = "ui/rmlui/overlay/screen_fade.rml";
constexpr std::string_view FADE_OVERLAY_ID = "fade-overlay";
constexpr std::string_view OPAQUE_CLASS = "is-opaque";
constexpr std::string_view TRANSITION_EVENT = "transitionend";

[[nodiscard]] float sanitizeDuration(float seconds) {
    return seconds > 0.0f ? seconds : 0.0f;
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

    overlay_ = document_->GetElementById(Rml::String{FADE_OVERLAY_ID.data(), FADE_OVERLAY_ID.size()});
    if (!overlay_) {
        spdlog::error("RmlScreenFade: #fade-overlay element not found.");
        runtime_.unloadDocument(document_);
        document_ = nullptr;
        return;
    }

    setTransitionDuration(0.0f);
    setOverlayOpaque(false);
    setOverlayInteractive(false);
    registerListeners();
    runtime_.hideDocument(document_);
    spdlog::debug("RmlScreenFade initialized.");
}

RmlScreenFade::~RmlScreenFade() {
    unregisterListeners();
    if (document_) {
        runtime_.unloadDocument(document_);
        document_ = nullptr;
    }
}

void RmlScreenFade::fadeOut(float seconds) {
    startFade(Phase::FadingOut, true, seconds);
}

void RmlScreenFade::fadeIn(float seconds) {
    startFade(Phase::FadingIn, false, seconds);
}

void RmlScreenFade::update(float delta_time) {
    (void)delta_time;
}

void RmlScreenFade::ProcessEvent(Rml::Event& event) {
    if (!overlay_ || event.GetTargetElement() != overlay_) {
        return;
    }

    const Rml::String property_name = event.GetParameter<Rml::String>("property", "");
    if (property_name != "opacity") {
        return;
    }

    completeTransition();
}

void RmlScreenFade::startFade(Phase next_phase, bool target_opaque, float seconds) {
    if (!document_ || !overlay_) {
        phase_ = next_phase;
        completeTransition();
        return;
    }

    phase_ = next_phase;
    runtime_.showDocument(document_);
    setOverlayInteractive(true);
    setTransitionDuration(seconds);

    if (overlay_opaque_ == target_opaque || sanitizeDuration(seconds) <= 0.0f) {
        setOverlayOpaque(target_opaque);
        completeTransition();
        return;
    }

    setOverlayOpaque(target_opaque);
}

void RmlScreenFade::setOverlayOpaque(bool opaque) {
    overlay_opaque_ = opaque;
    if (!overlay_) {
        return;
    }
    overlay_->SetClass(Rml::String{OPAQUE_CLASS.data(), OPAQUE_CLASS.size()}, opaque);
}

void RmlScreenFade::setTransitionDuration(float seconds) {
    if (!overlay_) {
        return;
    }

    const float duration = sanitizeDuration(seconds);
    if (duration <= 0.0f) {
        overlay_->SetProperty("transition", "none");
        return;
    }

    overlay_->SetProperty("transition", std::format("opacity {:.3f}s linear-in-out", duration));
}

void RmlScreenFade::completeTransition() {
    if (phase_ == Phase::FadingOut) {
        phase_ = Phase::Holding;
        return;
    }

    if (phase_ != Phase::FadingIn) {
        return;
    }

    phase_ = Phase::Idle;
    setOverlayInteractive(false);
    if (document_) {
        runtime_.hideDocument(document_);
    }
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

void RmlScreenFade::registerListeners() {
    if (!overlay_ || transition_listener_registered_) {
        return;
    }

    overlay_->AddEventListener(Rml::String{TRANSITION_EVENT.data(), TRANSITION_EVENT.size()}, this);
    transition_listener_registered_ = true;
}

void RmlScreenFade::unregisterListeners() {
    if (!overlay_ || !transition_listener_registered_) {
        return;
    }

    overlay_->RemoveEventListener(Rml::String{TRANSITION_EVENT.data(), TRANSITION_EVENT.size()}, this);
    transition_listener_registered_ = false;
}

} // namespace engine::ui::rmlui
