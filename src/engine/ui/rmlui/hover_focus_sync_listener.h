#pragma once

#include <RmlUi/Core/EventListener.h>

#include <functional>
#include <string>
#include <vector>

namespace Rml {
class Element;
class Event;
}

namespace engine::ui::rmlui {

class RmlUiRuntime;

class HoverFocusSyncListener final : public Rml::EventListener {
public:
    using CandidateFilter = std::function<bool(Rml::Element*)>;

    explicit HoverFocusSyncListener(RmlUiRuntime& runtime, CandidateFilter candidate_filter = {});

    void setCandidateFilter(CandidateFilter candidate_filter);

    /// 将此 listener 注册到元素的指定事件上，并记录注册信息。
    void registerTo(Rml::Element* element, std::string_view event_type);

    /// 移除所有通过 registerTo() 注册的事件监听器。必须在文档卸载前调用。
    void unregisterAll();

    void ProcessEvent(Rml::Event& event) override;

private:
    struct Registration {
        Rml::Element* element;
        std::string event_type;
    };

    RmlUiRuntime* runtime_{nullptr};
    CandidateFilter candidate_filter_{};
    std::vector<Registration> registrations_;
};

} // namespace engine::ui::rmlui
