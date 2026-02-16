#include "input_debug_panel.h"
#include <imgui.h>

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
    const auto& states = input_manager_.getActionStatesDebug();
    const auto& names = input_manager_.getActionNamesDebug();
    if (states.empty()) {
        ImGui::TextDisabled("<no actions>");
    } else {
        if (ImGui::BeginTable("InputActionsTable", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
            ImGui::TableSetupColumn("Action");
            ImGui::TableSetupColumn("ID");
            ImGui::TableSetupColumn("State");
            ImGui::TableSetupColumn("Press");
            ImGui::TableSetupColumn("Release");
            ImGui::TableHeadersRow();

            for (const auto& [id, state] : states) {
                const char* action_name = "<unnamed>";
                if (auto it = names.find(id); it != names.end()) {
                    action_name = it->second.c_str();
                }

                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(action_name);

                ImGui::TableSetColumnIndex(1);
                ImGui::Text("0x%X", static_cast<unsigned int>(id));

                ImGui::TableSetColumnIndex(2);
                ImGui::TextUnformatted(actionStateToString(state));

                ImGui::TableSetColumnIndex(3);
                ImGui::PushID(static_cast<int>(id));
                if (ImGui::Button("Press")) {
                    input_manager_.setActionStateDebug(id, engine::input::ActionState::PRESSED);
                }
                ImGui::SameLine();
                if (ImGui::Button("Hold")) {
                    input_manager_.setActionStateDebug(id, engine::input::ActionState::HELD);
                }
                ImGui::PopID();

                ImGui::TableSetColumnIndex(4);
                ImGui::PushID(static_cast<int>(id) + 10000);
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
