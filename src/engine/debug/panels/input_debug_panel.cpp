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

const char* inputDeviceToString(engine::input::InputDevice device) {
    switch (device) {
        case engine::input::InputDevice::KeyboardMouse: return "KeyboardMouse";
        case engine::input::InputDevice::Gamepad: return "Gamepad";
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
    const auto gamepad = input_manager_.getGamepadDebugState();

    ImGui::Text("Mouse Position: (%.1f, %.1f)", mouse_pos.x, mouse_pos.y);
    ImGui::Text("Logical Position: (%.1f, %.1f)", logical_pos.x, logical_pos.y);
    ImGui::Text("Wheel Delta: (%.1f, %.1f)", wheel.x, wheel.y);
    ImGui::Text("Last Input Device: %s", inputDeviceToString(gamepad.last_input_device));

    ImGui::Separator();
    ImGui::Text("Gamepad:");
    ImGui::Indent();
    ImGui::Text("Connected Count: %zu", gamepad.connected_gamepad_count);
    if (gamepad.has_active_gamepad) {
        ImGui::Text("Active Gamepad: %s", gamepad.active_gamepad_name.c_str());
        ImGui::Text("Instance ID: %d", static_cast<int>(gamepad.active_gamepad_id));
    } else {
        ImGui::TextDisabled("Active Gamepad: <none>");
    }

    if (ImGui::BeginTable("GamepadButtonsTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Button");
        ImGui::TableSetupColumn("Down");
        ImGui::TableHeadersRow();

        for (std::size_t i = 0; i < engine::input::GAMEPAD_BUTTON_COUNT; ++i) {
            const auto button = static_cast<SDL_GamepadButton>(i);
            const char* name = SDL_GetGamepadStringForButton(button);
            if (name == nullptr || name[0] == '\0') {
                continue;
            }

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(name);
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(gamepad.button_states[i] ? "Yes" : "No");
        }

        ImGui::EndTable();
    }

    if (ImGui::BeginTable("GamepadAxesTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Axis");
        ImGui::TableSetupColumn("Raw");
        ImGui::TableSetupColumn("Normalized");
        ImGui::TableHeadersRow();

        for (std::size_t i = 0; i < engine::input::GAMEPAD_AXIS_COUNT; ++i) {
            const auto axis = static_cast<SDL_GamepadAxis>(i);
            const char* name = SDL_GetGamepadStringForAxis(axis);
            if (name == nullptr || name[0] == '\0') {
                continue;
            }

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(name);
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%d", static_cast<int>(gamepad.axis_raw_values[i]));
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%.3f", gamepad.axis_normalized_values[i]);
        }

        ImGui::EndTable();
    }
    ImGui::Unindent();

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
