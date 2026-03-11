#pragma once

#include <RmlUi/Core/EventListener.h>

#include <functional>

namespace Rml {
class Element;
class Event;
}

namespace engine::ui::rmlui {

class RmlUILayer;

class HoverFocusSyncListener final : public Rml::EventListener {
public:
    using CandidateFilter = std::function<bool(Rml::Element*)>;

    explicit HoverFocusSyncListener(RmlUILayer& layer, CandidateFilter candidate_filter = {});

    void setCandidateFilter(CandidateFilter candidate_filter);
    void ProcessEvent(Rml::Event& event) override;

private:
    RmlUILayer* layer_{nullptr};
    CandidateFilter candidate_filter_{};
};

} // namespace engine::ui::rmlui
