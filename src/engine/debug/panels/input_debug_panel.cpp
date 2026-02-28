#include "input_debug_panel.h"
#include <imgui.h>

#include <cstdint>

namespace engine::debug {

namespace {
const char* actionStateToString(engine::input::ActionState state) {
    switch (state) {
        case engine::input::ActionState::PRESSED: return "Pressed";
        case engine::input::ActionState::HELD: return "Held";
        case engine::input::ActionState::RELEASED: return "Released";
        case engine::input::ActionState::INACTIVE: return "Inactive";
        default: return "Unknown";
    }
}

[[nodiscard]] unsigned long long toUnsignedLongLong(entt::id_type id) {
    return static_cast<unsigned long long>(id);
}

[[nodiscard]] const void* makeImGuiId(entt::id_type id, std::uintptr_t salt) {
    const auto raw = static_cast<std::uintptr_t>(id) ^ salt;
    return reinterpret_cast<const void*>(raw);
}
}

InputDebugPanel::InputDebugPanel(engine::input::InputManager& input_manager)
    : input_manager_(input_manager) {
}

std::string_view InputDebugPanel::name() const {
    return "Input";
}

void InputDebugPanel::draw(bool& is_open) {
    if (!is_open) {
        return;
    }

    if (!ImGui::Begin("Input Debug", &is_open, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End();
        return;
    }

    const auto mouse_pos = input_manager_.getMousePosition();
    const auto logical_pos = input_manager_.getLogicalMousePosition();
    const auto wheel = input_manager_.getMouseWheelDelta();

    ImGui::Text("Mouse Position: (%.1f, %.1f)", mouse_pos.x, mouse_pos.y);
    ImGui::Text("Logical Position: (%.1f, %.1f)", logical_pos.x, logical_pos.y);
    ImGui::Text("Wheel Delta: (%.1f, %.1f)", wheel.x, wheel.y);

    ImGui::Separator();
    ImGui::Text("Action States:");
    ImGui::Indent();
    const auto& actions = input_manager_.getActionsDebug();
    if (actions.empty()) {
        ImGui::TextDisabled("<no actions>");
    } else {
        if (ImGui::BeginTable("InputActionsTable", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
            ImGui::TableSetupColumn("Action");
            ImGui::TableSetupColumn("ID");
            ImGui::TableSetupColumn("State");
            ImGui::TableSetupColumn("Keys");
            ImGui::TableSetupColumn("Press");
            ImGui::TableSetupColumn("Release");
            ImGui::TableHeadersRow();

            for (const auto& [id, entry] : actions) {
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(entry.name.c_str());

                ImGui::TableSetColumnIndex(1);
                ImGui::Text("0x%016llX", toUnsignedLongLong(id));

                ImGui::TableSetColumnIndex(2);
                ImGui::TextUnformatted(actionStateToString(entry.state));

                ImGui::TableSetColumnIndex(3);
                ImGui::Text("%u", static_cast<unsigned>(entry.active_count));

                ImGui::TableSetColumnIndex(4);
                ImGui::PushID(makeImGuiId(id, 0u));
                if (ImGui::Button("Press")) {
                    input_manager_.setActionStateDebug(id, engine::input::ActionState::PRESSED);
                }
                ImGui::SameLine();
                if (ImGui::Button("Hold")) {
                    input_manager_.setActionStateDebug(id, engine::input::ActionState::HELD);
                }
                ImGui::PopID();

                ImGui::TableSetColumnIndex(5);
                ImGui::PushID(makeImGuiId(id, 1u));
                if (ImGui::Button("Release")) {
                    input_manager_.setActionStateDebug(id, engine::input::ActionState::RELEASED);
                }
                ImGui::PopID();
            }

            ImGui::EndTable();
        }
    }
    ImGui::Unindent();

    ImGui::End();
}

} // namespace engine::debug
