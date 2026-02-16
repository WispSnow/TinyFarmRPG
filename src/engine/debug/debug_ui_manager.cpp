#include "debug_ui_manager.h"

#include "dispatcher_trace.h"
#include "panels/dispatcher_trace_debug_panel.h"
#include "engine/scene/scene.h"
#include "engine/utils/events.h"

#include <algorithm>
#include <imgui.h>

namespace engine::debug {

DebugUIManager::~DebugUIManager() = default;

void DebugUIManager::enableDispatcherTrace(entt::dispatcher& dispatcher) {
    if (dispatcher_trace_) {
        return;
    }

    dispatcher_trace_ = std::make_unique<DispatcherTrace>();

    dispatcher_trace_->watch<engine::utils::QuitEvent>(dispatcher, "QuitEvent");
    dispatcher_trace_->watch<engine::utils::WindowResizedEvent>(dispatcher, "WindowResizedEvent");
    dispatcher_trace_->watch<engine::utils::PushSceneEvent>(dispatcher, "PushSceneEvent");
    dispatcher_trace_->watch<engine::utils::PopSceneEvent>(dispatcher, "PopSceneEvent");
    dispatcher_trace_->watch<engine::utils::ReplaceSceneEvent>(dispatcher, "ReplaceSceneEvent");

    dispatcher_trace_->watch<engine::utils::PlayAnimationEvent>(dispatcher, "PlayAnimationEvent");
    dispatcher_trace_->watch<engine::utils::AnimationEvent>(dispatcher, "AnimationEvent");
    dispatcher_trace_->watch<engine::utils::AnimationFinishedEvent>(dispatcher, "AnimationFinishedEvent");

    dispatcher_trace_->watch<engine::utils::PlaySoundEvent>(dispatcher, "PlaySoundEvent");

    dispatcher_trace_->watch<engine::utils::FontUnloadedEvent>(dispatcher, "FontUnloadedEvent");
    dispatcher_trace_->watch<engine::utils::FontsClearedEvent>(dispatcher, "FontsClearedEvent");

    dispatcher_trace_->watch<engine::utils::AddAutoTileEntityEvent>(dispatcher, "AddAutoTileEntityEvent");
    dispatcher_trace_->watch<engine::utils::RemoveAutoTileEntityEvent>(dispatcher, "RemoveAutoTileEntityEvent");

    dispatcher_trace_->watch<engine::utils::DayChangedEvent>(dispatcher, "DayChangedEvent");
    dispatcher_trace_->watch<engine::utils::HourChangedEvent>(dispatcher, "HourChangedEvent");
    dispatcher_trace_->watch<engine::utils::TimeOfDayChangedEvent>(dispatcher, "TimeOfDayChangedEvent");

    registerPanel(std::make_unique<DispatcherTraceDebugPanel>(*dispatcher_trace_), false, PanelCategory::Engine);
}

void DebugUIManager::onDispatcherUpdateBegin() {
    if (dispatcher_trace_) {
        dispatcher_trace_->beginQueueDispatch();
    }
}

void DebugUIManager::onDispatcherUpdateEnd() {
    if (dispatcher_trace_) {
        dispatcher_trace_->endQueueDispatch();
    }
}

void DebugUIManager::toggleVisible(PanelCategory category) {
    const size_t index = static_cast<size_t>(category);
    visible_[index] = !visible_[index];
}

void DebugUIManager::setVisible(bool visible, PanelCategory category) {
    const size_t index = static_cast<size_t>(category);
    visible_[index] = visible;
}

DebugPanel* DebugUIManager::registerPanel(std::unique_ptr<DebugPanel> panel, bool enabled, PanelCategory category) {
    if (!panel) {
        return nullptr;
    }

    PanelEntry entry;
    entry.name = std::string(panel->name());
    entry.enabled = enabled;
    auto* raw_ptr = panel.get();
    entry.panel = std::move(panel);

    if (entry.enabled) {
        entry.panel->onShow();
    }

    const size_t index = static_cast<size_t>(category);
    panels_[index].push_back(std::move(entry));
    return raw_ptr;
}

void DebugUIManager::unregisterPanel(const DebugPanel* panel, PanelCategory category) {
    if (!panel) {
        return;
    }

    const size_t index = static_cast<size_t>(category);
    auto& panel_list = panels_[index];
    auto it = std::find_if(panel_list.begin(), panel_list.end(), [panel](const PanelEntry& entry) {
        return entry.panel.get() == panel;
    });

    if (it == panel_list.end()) {
        return;
    }

    if (it->enabled) {
        it->panel->onHide();
    }

    panel_list.erase(it);
}

void DebugUIManager::unregisterPanels(PanelCategory category) {
    const size_t index = static_cast<size_t>(category);
    auto& panel_list = panels_[index];
    for (auto& entry : panel_list) {
        if (entry.enabled) {
            entry.panel->onHide();
        }
    }
    panel_list.clear();
}

void DebugUIManager::draw() {
    // 绘制所有类别的控制面板
    for (size_t i = 0; i < PANEL_CATEGORY_COUNT; ++i) {
        drawHubWindow(static_cast<PanelCategory>(i));
    }

    // 绘制所有类别的调试面板
    for (size_t i = 0; i < PANEL_CATEGORY_COUNT; ++i) {
        for (auto& entry : panels_[i]) {
            if (!entry.enabled) {
                continue;
            }

            bool is_open = true;
            entry.panel->draw(is_open);
            if (!is_open) {
                entry.panel->onHide();
                entry.enabled = false;
            }
        }
    }
}

void DebugUIManager::drawHubWindow(PanelCategory category) {
    const size_t index = static_cast<size_t>(category);
    if (!visible_[index]) {
        return;
    }

    const char* window_title = getCategoryName(category);
    bool open = visible_[index];
    if (!ImGui::Begin(window_title, &open, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End();
        visible_[index] = open;
        return;
    }

    visible_[index] = open;
    auto& panel_list = panels_[index];
    for (auto& entry : panel_list) {
        bool previous = entry.enabled;
        if (ImGui::Checkbox(entry.name.c_str(), &entry.enabled)) {
            if (entry.enabled && !previous) {
                entry.panel->onShow();
            } else if (!entry.enabled && previous) {
                entry.panel->onHide();
            }
        }
    }

    ImGui::End();
}

} // namespace engine::debug
