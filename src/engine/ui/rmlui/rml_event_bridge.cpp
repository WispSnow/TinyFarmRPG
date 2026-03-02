#include "engine/ui/rmlui/rml_event_bridge.h"

#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/Event.h>
#include <spdlog/spdlog.h>

namespace engine::ui::rmlui {

void RmlEventBridge::on(std::string_view name, Callback callback) {
    callbacks_[std::string(name)] = std::move(callback);
}

void RmlEventBridge::registerTo(Rml::Element* element, std::string_view event_type) {
    if (!element) {
        return;
    }
    const Rml::String type{event_type.data(), event_type.size()};
    element->AddEventListener(type, this);
}

void RmlEventBridge::ProcessEvent(Rml::Event& event) {
    // 从触发元素向上遍历祖先链，查找最近的 "data-command" 属性。
    // 这样即使点击按钮内部子节点（图标/文本 span），也能正确触发回调。
    auto* element = event.GetTargetElement();
    Rml::String command;

    while (element) {
        command = element->GetAttribute<Rml::String>("data-command", "");
        if (!command.empty()) {
            break;
        }
        element = element->GetParentNode();
    }

    if (command.empty()) {
        return;
    }

    auto it = callbacks_.find(std::string(command.c_str()));
    if (it != callbacks_.end()) {
        it->second(event);
    } else {
        spdlog::debug("RmlEventBridge: unhandled command '{}'.", command.c_str());
    }
}

} // namespace engine::ui::rmlui
