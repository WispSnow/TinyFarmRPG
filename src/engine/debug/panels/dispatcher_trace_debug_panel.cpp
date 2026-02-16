#include "dispatcher_trace_debug_panel.h"

#include "engine/debug/dispatcher_trace.h"

#include <imgui.h>

namespace engine::debug {

DispatcherTraceDebugPanel::DispatcherTraceDebugPanel(DispatcherTrace& trace)
    : trace_(trace) {
}

std::string_view DispatcherTraceDebugPanel::name() const {
    return "Core: Dispatcher Trace";
}

void DispatcherTraceDebugPanel::draw(bool& is_open) {
    if (!is_open) {
        return;
    }

    if (!ImGui::Begin("Dispatcher Trace", &is_open, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End();
        return;
    }

    ImGui::TextUnformatted("Immediate: dispatched outside dispatcher.update() (typically trigger)");
    ImGui::TextUnformatted("Queued: dispatched during dispatcher.update() (typically enqueue)");
    ImGui::Separator();

    if (ImGui::SliderInt("Recent Dispatches", &recent_dispatches_, 1, 200)) {
        if (recent_dispatches_ < 1) recent_dispatches_ = 1;
    }

    const auto recent = trace_.recentDispatches(static_cast<std::size_t>(recent_dispatches_));
    if (recent.empty()) {
        ImGui::TextUnformatted("No dispatches recorded yet.");
        ImGui::End();
        return;
    }

    ImGui::BeginChild("dispatcher_trace_history", ImVec2(0, 260), true);
    if (ImGui::BeginTable("dispatcher_trace_table", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
        ImGui::TableSetupColumn("Seq", ImGuiTableColumnFlags_WidthFixed, 70.0f);
        ImGui::TableSetupColumn("Timing", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("Event");
        ImGui::TableHeadersRow();

        for (const auto& e : recent) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%llu", static_cast<unsigned long long>(e.sequence));
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(e.queued ? "Queued" : "Immediate");
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(e.event_name.data(), e.event_name.data() + e.event_name.size());
        }

        ImGui::EndTable();
    }
    ImGui::EndChild();

    ImGui::End();
}

} // namespace engine::debug
