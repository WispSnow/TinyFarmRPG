#include "time_debug_panel.h"
#include "engine/core/time.h"
#include <imgui.h>

namespace engine::debug {

TimeDebugPanel::TimeDebugPanel(engine::core::Time& time)
    : time_(time) {
}

std::string_view TimeDebugPanel::name() const {
    return "Core: Time";
}

void TimeDebugPanel::draw(bool& is_open) {
    if (!is_open) {
        return;
    }

    if (!ImGui::Begin("Time Debug", &is_open, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End();
        return;
    }

    const float delta = time_.getDeltaTime();
    const float unscaled = time_.getUnscaledDeltaTime();
    const float fps = delta > 0.0f ? (1.0f / delta) : 0.0f;
    ImGui::Text("Delta Time: %.3f ms (%.3f ms raw)", delta * 1000.0f, unscaled * 1000.0f);
    ImGui::Text("Estimated FPS: %.1f", fps);
    ImGui::Separator();

    float scale = time_scale_;
    if (ImGui::SliderFloat("Time Scale", &scale, 0.1f, 4.0f, "%.2f")) {
        time_scale_ = scale;
        time_.setTimeScale(time_scale_);
    }

    int fps_target = target_fps_;
    if (ImGui::SliderInt("Target FPS", &fps_target, 0, 240)) {
        target_fps_ = fps_target;
        time_.setTargetFps(target_fps_);
    }

    ImGui::End();
}

void TimeDebugPanel::onShow() {
    syncFromTime();
}

void TimeDebugPanel::syncFromTime() {
    time_scale_ = time_.getTimeScale();
    target_fps_ = time_.getTargetFps();
}

} // namespace engine::debug
