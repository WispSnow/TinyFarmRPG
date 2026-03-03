#pragma once

#include "engine/scene/scene.h"
#include "engine/ui/rmlui/rml_event_bridge.h"

#include <RmlUi/Core/EventListener.h>

namespace Rml {
class ElementDocument;
class Element;
} // namespace Rml

namespace learn::rmlui {

/// 点击计数器——直接实现 Rml::EventListener
class ClickCounter final : public Rml::EventListener {
public:
    void ProcessEvent(Rml::Event& event) override;

private:
    int count_ = 0;
};

/// 悬停信息——mouseover 事件监听
class HoverInfoListener final : public Rml::EventListener {
public:
    explicit HoverInfoListener(Rml::Element* info_display);
    void ProcessEvent(Rml::Event& event) override;

private:
    Rml::Element* info_display_;
};

/// 事件系统演示场景
class EventsScene final : public engine::scene::Scene {
public:
    using Scene::Scene;

    [[nodiscard]] bool init() override;
    void clean() override;

private:
    void setupClickCounter();
    void setupCommandMenu();
    void setupHoverInfo();
    void removeAllListeners();

    Rml::ElementDocument* doc_ = nullptr;
    ClickCounter click_counter_;
    HoverInfoListener* hover_listener_ = nullptr;
    engine::ui::rmlui::RmlEventBridge command_bridge_;
};

} // namespace learn::rmlui
