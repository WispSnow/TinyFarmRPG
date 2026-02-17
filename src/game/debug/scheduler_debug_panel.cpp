#include "scheduler_debug_panel.h"

#include "scheduler_profiler.h"

#include <imgui.h>

namespace game::debug {

SchedulerDebugPanel::SchedulerDebugPanel(SchedulerProfiler& profiler, const game::runtime::GameMode* current_mode)
    : profiler_(profiler), current_mode_(current_mode) {
}

std::string_view SchedulerDebugPanel::name() const {
    return "Scheduler";
}

void SchedulerDebugPanel::draw(bool& is_open) {
    if (!is_open) {
        return;
    }

    if (!ImGui::Begin("Scheduler Debug", &is_open, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End();
        return;
    }

    const auto mode = current_mode_ ? *current_mode_ : game::runtime::GameMode::Exploration;
    ImGui::Text("mode: %s", game::runtime::toString(mode));

    bool capture_enabled = profiler_.isEnabled();
    if (ImGui::Checkbox("Capture", &capture_enabled)) {
        profiler_.setEnabled(capture_enabled);
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
        profiler_.clear();
    }

    if (ImGui::SliderInt("Recent Frames", &recent_frames_, 1, 240)) {
        if (recent_frames_ < 1) {
            recent_frames_ = 1;
        }
    }

    ImGui::Text("history: %zu / %zu", profiler_.frameCount(), profiler_.maxFrames());
    ImGui::Separator();

    const auto* latest = profiler_.latestFrame();
    if (!latest) {
        ImGui::TextUnformatted("No sampled frame yet. Enable Capture first.");
        ImGui::End();
        return;
    }

    ImGui::Text("latest_total: %.3f ms", latest->total_ms);
    ImGui::Text("latest_gate1: %s", latest->gate1_triggered ? "true" : "false");
    ImGui::Text("latest_gate2: %s", latest->gate2_triggered ? "true" : "false");
    ImGui::Text("latest_stage_count: %zu", latest->stages.size());

    if (ImGui::CollapsingHeader("Latest Frame Stages", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::BeginChild("scheduler_latest_stages", ImVec2(0, 220), true);
        if (ImGui::BeginTable("scheduler_latest_stage_table",
                              3,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
            ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 30.0f);
            ImGui::TableSetupColumn("Stage");
            ImGui::TableSetupColumn("Elapsed (ms)", ImGuiTableColumnFlags_WidthFixed, 100.0f);
            ImGui::TableHeadersRow();

            for (std::size_t i = 0; i < latest->stages.size(); ++i) {
                const auto& stage = latest->stages[i];
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%zu", i);
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(game::runtime::toString(stage.stage));
                ImGui::TableNextColumn();
                ImGui::Text("%.3f", stage.elapsed_ms);
            }

            ImGui::EndTable();
        }
        ImGui::EndChild();
    }

    if (ImGui::CollapsingHeader("Recent Average Summary", ImGuiTreeNodeFlags_DefaultOpen)) {
        const auto aggregates = profiler_.aggregateRecent(static_cast<std::size_t>(recent_frames_));

        if (aggregates.empty()) {
            ImGui::TextUnformatted("No aggregate data.");
        } else if (ImGui::BeginTable("scheduler_aggregate_table",
                                     4,
                                     ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Stage");
            ImGui::TableSetupColumn("Avg (ms)", ImGuiTableColumnFlags_WidthFixed, 90.0f);
            ImGui::TableSetupColumn("Max (ms)", ImGuiTableColumnFlags_WidthFixed, 90.0f);
            ImGui::TableSetupColumn("Samples", ImGuiTableColumnFlags_WidthFixed, 70.0f);
            ImGui::TableHeadersRow();

            for (const auto& agg : aggregates) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(game::runtime::toString(agg.stage));
                ImGui::TableNextColumn();
                ImGui::Text("%.3f", agg.avg_ms);
                ImGui::TableNextColumn();
                ImGui::Text("%.3f", agg.max_ms);
                ImGui::TableNextColumn();
                ImGui::Text("%zu", agg.samples);
            }

            ImGui::EndTable();
        }
    }

    ImGui::End();
}

} // namespace game::debug
