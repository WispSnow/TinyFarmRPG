#include "time_debug_panel.h"
#include "engine/core/time.h"
#include <cmath>
#include <imgui.h>

namespace engine::debug {

TimeDebugPanel::TimeDebugPanel(engine::core::Time& time, bool& render_interpolation_enabled)
    : time_(time),
      render_interpolation_enabled_(render_interpolation_enabled) {
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

    const float scaled_delta = time_.getDeltaTime();
    const float unscaled_delta = time_.getUnscaledDeltaTime();
    const float render_fps = unscaled_delta > 0.0f ? (1.0f / unscaled_delta) : 0.0f;
    const float fixed_delta = time_.getFixedDeltaTime();
    const float logic_hz = fixed_delta > 0.0f ? (1.0f / fixed_delta) : 0.0f;
    const float accumulator = time_.getAccumulator();
    const float backlog_ticks = fixed_delta > 0.0f ? (accumulator / fixed_delta) : 0.0f;
    const float interpolation_alpha = time_.getInterpolationAlpha();
    const float effective_render_alpha = render_interpolation_enabled_ ? interpolation_alpha : 1.0f;

    ImGui::Text("Frame Delta: %.3f ms (scaled: %.3f ms)", unscaled_delta * 1000.0f, scaled_delta * 1000.0f);
    ImGui::Text("Render FPS: %.1f", render_fps);
    ImGui::Text("Logic Step: %.3f ms (%.1f Hz)", fixed_delta * 1000.0f, logic_hz);
    ImGui::Text("Fixed Ticks This Frame: %d", time_.getFixedTicksThisFrame());
    ImGui::Text("Accumulator: %.3f ms (%.2f ticks)", accumulator * 1000.0f, backlog_ticks);
    ImGui::Text("Interpolation Alpha (raw): %.3f", interpolation_alpha);
    ImGui::Text("Interpolation Alpha (effective): %.3f", effective_render_alpha);
    ImGui::Text("Render Interpolation: %s", render_interpolation_enabled_ ? "Enabled" : "Disabled");
    ImGui::Text("Catch-up Clamped This Frame: %s", time_.wasCatchUpClampedThisFrame() ? "Yes" : "No");
    ImGui::Text("Dropped Fixed Ticks (Total): %llu",
                static_cast<unsigned long long>(time_.getDroppedFixedTicksTotal()));
    ImGui::Separator();

    float scale = time_scale_;
    if (ImGui::SliderFloat("Time Scale", &scale, 0.0f, 4.0f, "%.2f")) {
        time_scale_ = scale;
        time_.setTimeScale(time_scale_);
    }

    int render_target_fps = target_fps_;
    if (ImGui::SliderInt("Render Target FPS", &render_target_fps, 0, 240)) {
        target_fps_ = render_target_fps;
        time_.setTargetFps(target_fps_);
    }

    int logic_hz_value = logic_tick_hz_;
    if (ImGui::SliderInt("Logic Tick Hz", &logic_hz_value, 1, 240)) {
        logic_tick_hz_ = logic_hz_value;
        time_.setFixedDeltaTime(1.0f / static_cast<float>(logic_tick_hz_));
    }

    int max_ticks = max_ticks_per_frame_;
    if (ImGui::SliderInt("Max Ticks Per Frame", &max_ticks, 1, 20)) {
        max_ticks_per_frame_ = max_ticks;
        time_.setMaxTicksPerFrame(max_ticks_per_frame_);
    }

    bool render_interpolation_enabled = render_interpolation_enabled_;
    if (ImGui::Checkbox("Enable Render Interpolation", &render_interpolation_enabled)) {
        render_interpolation_enabled_ = render_interpolation_enabled;
    }

    ImGui::End();
}

void TimeDebugPanel::onShow() {
    syncFromTime();
}

void TimeDebugPanel::syncFromTime() {
    time_scale_ = time_.getTimeScale();
    target_fps_ = time_.getTargetFps();
    const float fixed_delta = time_.getFixedDeltaTime();
    logic_tick_hz_ = fixed_delta > 0.0f ? static_cast<int>(std::lround(1.0f / fixed_delta)) : 60;
    if (logic_tick_hz_ < 1) {
        logic_tick_hz_ = 1;
    }
    max_ticks_per_frame_ = time_.getMaxTicksPerFrame();
}

} // namespace engine::debug
