#include "engine/ui/rmlui/hover_focus_sync_listener.h"

#include "engine/ui/rmlui/rml_ui_layer.h"

#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/Event.h>

#include <utility>

namespace engine::ui::rmlui {

namespace {

[[nodiscard]] bool isHoverFocusableButton(const Rml::Element* element) {
    return element != nullptr && element->GetTagName() == "button";
}

} // namespace

HoverFocusSyncListener::HoverFocusSyncListener(RmlUILayer& layer, CandidateFilter candidate_filter)
    : layer_(&layer),
      candidate_filter_(std::move(candidate_filter)) {
}

void HoverFocusSyncListener::setCandidateFilter(CandidateFilter candidate_filter) {
    candidate_filter_ = std::move(candidate_filter);
}

void HoverFocusSyncListener::ProcessEvent(Rml::Event& event) {
    if (!layer_) {
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
        if (layer_->getFocusedElement() == element) {
            return;
        }
        if (layer_->focusElement(element)) {
            return;
        }
    }
}

} // namespace engine::ui::rmlui
