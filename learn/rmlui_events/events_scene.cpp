#include "events_scene.h"

#include "engine/core/context.h"
#include "engine/render/opengl/gl_renderer.h"

#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Event.h>
#include <spdlog/spdlog.h>

namespace learn::rmlui {

// --- ClickCounter ---

void ClickCounter::ProcessEvent(Rml::Event& event) {
    if (event.GetId() != Rml::EventId::Click) {
        return;
    }

    ++count_;
    spdlog::info("Click count: {}", count_);

    auto* target = event.GetTargetElement();
    if (!target) {
        return;
    }
    auto* doc = target->GetOwnerDocument();
    if (auto* display = doc->GetElementById("click-count")) {
        display->SetInnerRML(Rml::String(std::to_string(count_)));
    }
}

// --- HoverInfoListener ---

HoverInfoListener::HoverInfoListener(Rml::Element* info_display)
    : info_display_(info_display) {}

void HoverInfoListener::ProcessEvent(Rml::Event& event) {
    if (event.GetId() != Rml::EventId::Mouseover) {
        return;
    }

    auto* target = event.GetTargetElement();
    if (!target || !info_display_) {
        return;
    }

    auto info = target->GetAttribute<Rml::String>("data-info", "");
    if (!info.empty()) {
        info_display_->SetInnerRML(info);
        spdlog::info("Hover info: {}", info.c_str());
    }
}

// --- EventsScene ---

bool EventsScene::init() {
    if (!Scene::init()) {
        return false;
    }

    context_.getGLRenderer().setDebugUIEnabled(true);

    doc_ = loadRmlDocument("ui/rmlui/learn/learn_events.rml");
    if (!doc_) {
        spdlog::error("Failed to load learn_events.rml");
        return false;
    }

    setupClickCounter();
    setupCommandMenu();
    setupHoverInfo();

    spdlog::info("Events scene initialized.");
    return true;
}

void EventsScene::setupClickCounter() {
    if (auto* btn = doc_->GetElementById("click-btn")) {
        btn->AddEventListener(Rml::EventId::Click, &click_counter_);
    }
}

void EventsScene::setupCommandMenu() {
    command_bridge_.on("cmd_attack", [this](Rml::Event&) {
        spdlog::info("Command: Attack!");
        if (auto* log = doc_->GetElementById("cmd-log")) {
            log->SetInnerRML("&gt; Attack!");
        }
    });
    command_bridge_.on("cmd_magic", [this](Rml::Event&) {
        spdlog::info("Command: Magic!");
        if (auto* log = doc_->GetElementById("cmd-log")) {
            log->SetInnerRML("&gt; Magic!");
        }
    });
    command_bridge_.on("cmd_items", [this](Rml::Event&) {
        spdlog::info("Command: Items!");
        if (auto* log = doc_->GetElementById("cmd-log")) {
            log->SetInnerRML("&gt; Items!");
        }
    });
    command_bridge_.on("cmd_defend", [this](Rml::Event&) {
        spdlog::info("Command: Defend!");
        if (auto* log = doc_->GetElementById("cmd-log")) {
            log->SetInnerRML("&gt; Defend!");
        }
    });
    command_bridge_.on("cmd_flee", [this](Rml::Event&) {
        spdlog::info("Command: Flee!");
        if (auto* log = doc_->GetElementById("cmd-log")) {
            log->SetInnerRML("&gt; Flee!");
        }
    });

    if (auto* menu = doc_->GetElementById("cmd-menu")) {
        command_bridge_.registerTo(menu, "click");
    }
}

void EventsScene::setupHoverInfo() {
    auto* info_display = doc_->GetElementById("hover-display");
    if (!info_display) {
        return;
    }

    hover_listener_ = new HoverInfoListener(info_display);

    Rml::ElementList items;
    doc_->GetElementsByClassName(items, "info-item");
    for (auto* item : items) {
        item->AddEventListener(Rml::EventId::Mouseover, hover_listener_);
    }
}

void EventsScene::removeAllListeners() {
    if (!doc_) {
        return;
    }

    // 移除 click counter 监听器
    if (auto* btn = doc_->GetElementById("click-btn")) {
        btn->RemoveEventListener(Rml::EventId::Click, &click_counter_);
    }

    // 移除 command bridge 监听器
    if (auto* menu = doc_->GetElementById("cmd-menu")) {
        menu->RemoveEventListener("click", &command_bridge_);
    }

    // 移除 hover 监听器
    if (hover_listener_) {
        Rml::ElementList items;
        doc_->GetElementsByClassName(items, "info-item");
        for (auto* item : items) {
            item->RemoveEventListener(Rml::EventId::Mouseover, hover_listener_);
        }
    }
}

void EventsScene::clean() {
    // 必须先移除所有事件监听器，再卸载文档。
    // 否则文档卸载时可能触发 blur 等事件，回调访问已销毁的元素导致崩溃。
    removeAllListeners();

    unloadAllRmlDocuments();
    doc_ = nullptr;

    delete hover_listener_;
    hover_listener_ = nullptr;

    Scene::clean();
}

} // namespace learn::rmlui
