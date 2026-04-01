#include "engine/ui/rmlui/hover_focus_sync_listener.h"

#include "engine/ui/rmlui/rml_ui_runtime.h"

#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/Event.h>

#include <utility>

namespace engine::ui::rmlui {

namespace {

[[nodiscard]] bool isHoverFocusableButton(const Rml::Element* element) {
    return element != nullptr && element->GetTagName() == "button";
}

} // namespace

HoverFocusSyncListener::HoverFocusSyncListener(RmlUiRuntime& runtime, CandidateFilter candidate_filter)
    : runtime_(&runtime),
      candidate_filter_(std::move(candidate_filter)) {
}

void HoverFocusSyncListener::setCandidateFilter(CandidateFilter candidate_filter) {
    candidate_filter_ = std::move(candidate_filter);
}

void HoverFocusSyncListener::registerTo(Rml::Element* element, std::string_view event_type) {
    if (!element) {
        return;
    }
    const Rml::String type{event_type.data(), event_type.size()};
    element->AddEventListener(type, this);
    registrations_.push_back({element, std::string(event_type)});
}

void HoverFocusSyncListener::unregisterAll() {
    for (const auto& reg : registrations_) {
        const Rml::String type{reg.event_type.data(), reg.event_type.size()};
        reg.element->RemoveEventListener(type, this);
    }
    registrations_.clear();
}

void HoverFocusSyncListener::ProcessEvent(Rml::Event& event) {
    if (!runtime_) {
        return;
    }

    for (auto* element = event.GetTargetElement(); element != nullptr; element = element->GetParentNode()) {
        if (!isHoverFocusableButton(element) || !element->IsVisible(true)) {
            continue;
        }
        if (element->HasAttribute("disabled")) {
            continue;
        }
        if (candidate_filter_ && !candidate_filter_(element)) {
            continue;
        }
        if (runtime_->focusElement(element)) {
            return;
        }
    }
}

} // namespace engine::ui::rmlui
