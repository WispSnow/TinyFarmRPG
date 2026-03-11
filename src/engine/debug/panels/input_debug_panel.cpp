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
        case engine::input::InputDevice::Keyboard: return "Keyboard";
        case engine::input::InputDevice::Mouse: return "Mouse";
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
    const auto snapshot = input_manager_.getDebugSnapshot();

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
    ImGui::Text("Rumble:");
    ImGui::Indent();
    if (snapshot.rumble.has_last_request) {
        ImGui::Text("Last Request: intensity=%.2f duration=%ums success=%s",
                    snapshot.rumble.last_intensity,
                    snapshot.rumble.last_duration_ms,
                    snapshot.rumble.last_request_succeeded ? "true" : "false");
    } else {
        ImGui::TextDisabled("Last Request: <none>");
    }
    if (snapshot.rumble.active) {
        ImGui::Text("Active: intensity=%.2f remaining=%llums",
                    snapshot.rumble.current_intensity,
                    static_cast<unsigned long long>(snapshot.rumble.remaining_ms));
    } else {
        ImGui::TextDisabled("Active: <none>");
    }
    ImGui::Unindent();

    ImGui::Separator();
    ImGui::Text("Rebind:");
    ImGui::Indent();
    ImGui::Text("Capture Active: %s", snapshot.rebind.capture_active ? "Yes" : "No");
    if (snapshot.rebind.capture_active) {
        ImGui::Text("Target: %s[%zu]",
                    snapshot.rebind.action_name.c_str(),
                    snapshot.rebind.binding_index);
    }
    if (snapshot.rebind.pending_conflict) {
        ImGui::Text("Pending Token: %s", snapshot.rebind.pending_token.c_str());
        for (const auto& action_name : snapshot.rebind.conflicting_action_names) {
            ImGui::BulletText("%s", action_name.c_str());
        }
    }
    ImGui::Unindent();

    ImGui::Separator();
    ImGui::Text("Action States:");
    ImGui::Indent();
    if (snapshot.actions.empty()) {
        ImGui::TextDisabled("<no actions>");
    } else {
        if (ImGui::BeginTable("InputActionsTable", 8, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
            ImGui::TableSetupColumn("Action");
            ImGui::TableSetupColumn("ID");
            ImGui::TableSetupColumn("State");
            ImGui::TableSetupColumn("Bindings");
            ImGui::TableSetupColumn("Prompt");
            ImGui::TableSetupColumn("Buffer");
            ImGui::TableSetupColumn("Press");
            ImGui::TableSetupColumn("Release");
            ImGui::TableHeadersRow();

            for (const auto& action : snapshot.actions) {
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(action.name.c_str());

                ImGui::TableSetColumnIndex(1);
                ImGui::Text("0x%016llX", toUnsignedLongLong(action.id));

                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%s (%u)",
                            actionStateToString(action.state),
                            static_cast<unsigned>(action.active_count));

                ImGui::TableSetColumnIndex(3);
                if (action.bindings.empty()) {
                    ImGui::TextDisabled("<none>");
                } else {
                    for (std::size_t i = 0; i < action.bindings.size(); ++i) {
                        const auto& binding = action.bindings[i];
                        ImGui::Text("%s", binding.token.c_str());
                        if (i + 1 < action.bindings.size()) {
                            ImGui::Separator();
                        }
                    }
                }

                ImGui::TableSetColumnIndex(4);
                if (action.active_prompt.has_value()) {
                    ImGui::Text("%s", action.active_prompt->fallback_text.c_str());
                    ImGui::TextDisabled("%s", action.active_prompt->icon_id.c_str());
                } else {
                    ImGui::TextDisabled("<none>");
                }

                ImGui::TableSetColumnIndex(5);
                if (action.buffered_presses.empty()) {
                    ImGui::TextDisabled("<empty>");
                } else {
                    for (const auto& buffered_press : action.buffered_presses) {
                        ImGui::Text("%llums", static_cast<unsigned long long>(buffered_press.age_ms));
                    }
                }

                ImGui::TableSetColumnIndex(6);
                ImGui::PushID(makeImGuiId(action.id, 0u));
                if (ImGui::Button("Press")) {
                    input_manager_.setActionStateDebug(action.id, engine::input::ActionState::PRESSED);
                }
                ImGui::SameLine();
                if (ImGui::Button("Hold")) {
                    input_manager_.setActionStateDebug(action.id, engine::input::ActionState::HELD);
                }
                ImGui::PopID();

                ImGui::TableSetColumnIndex(7);
                ImGui::PushID(makeImGuiId(action.id, 1u));
                if (ImGui::Button("Release")) {
                    input_manager_.setActionStateDebug(action.id, engine::input::ActionState::RELEASED);
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
