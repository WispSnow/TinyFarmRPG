#include "engine/input/input_binding_tokens.h"

#include "engine/input/input_glyphs.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <string>

namespace engine::input {
namespace {

template <typename InputT>
[[nodiscard]] BindingDefinition makeBindingDefinition(InputDevice device, InputT physical_input, std::string_view token) {
    BindingDefinition binding;
    binding.token = std::string(token);
    binding.device = device;
    binding.physical_input = PhysicalInput{physical_input};
    binding.prompt_icon_id = buildPromptIconId(binding.device, binding.physical_input, binding.token);
    binding.prompt_fallback_text = buildPromptFallbackText(binding.device, binding.physical_input, binding.token);
    return binding;
}

[[nodiscard]] SDL_Scancode scancodeFromString(const std::string_view key_name) {
    return SDL_GetScancodeFromName(std::string(key_name).c_str());
}

[[nodiscard]] Uint32 mouseButtonFromString(const std::string_view button_name) {
    if (button_name == "MouseLeft") return SDL_BUTTON_LEFT;
    if (button_name == "MouseMiddle") return SDL_BUTTON_MIDDLE;
    if (button_name == "MouseRight") return SDL_BUTTON_RIGHT;
    if (button_name == "MouseX1") return SDL_BUTTON_X1;
    if (button_name == "MouseX2") return SDL_BUTTON_X2;
    return 0;
}

[[nodiscard]] SDL_GamepadButton gamepadButtonFromString(const std::string_view button_name) {
    if (button_name == "GamepadSouth") return SDL_GAMEPAD_BUTTON_SOUTH;
    if (button_name == "GamepadEast") return SDL_GAMEPAD_BUTTON_EAST;
    if (button_name == "GamepadWest") return SDL_GAMEPAD_BUTTON_WEST;
    if (button_name == "GamepadNorth") return SDL_GAMEPAD_BUTTON_NORTH;
    if (button_name == "GamepadBack") return SDL_GAMEPAD_BUTTON_BACK;
    if (button_name == "GamepadGuide") return SDL_GAMEPAD_BUTTON_GUIDE;
    if (button_name == "GamepadStart") return SDL_GAMEPAD_BUTTON_START;
    if (button_name == "GamepadLeftStick") return SDL_GAMEPAD_BUTTON_LEFT_STICK;
    if (button_name == "GamepadRightStick") return SDL_GAMEPAD_BUTTON_RIGHT_STICK;
    if (button_name == "GamepadLeftShoulder") return SDL_GAMEPAD_BUTTON_LEFT_SHOULDER;
    if (button_name == "GamepadRightShoulder") return SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER;
    if (button_name == "GamepadDpadUp") return SDL_GAMEPAD_BUTTON_DPAD_UP;
    if (button_name == "GamepadDpadDown") return SDL_GAMEPAD_BUTTON_DPAD_DOWN;
    if (button_name == "GamepadDpadLeft") return SDL_GAMEPAD_BUTTON_DPAD_LEFT;
    if (button_name == "GamepadDpadRight") return SDL_GAMEPAD_BUTTON_DPAD_RIGHT;
    return SDL_GAMEPAD_BUTTON_INVALID;
}

[[nodiscard]] std::optional<GamepadAxisDirection> gamepadAxisDirectionFromString(const std::string_view axis_name) {
    if (axis_name == "LeftStickUp") return GamepadAxisDirection::LeftStickUp;
    if (axis_name == "LeftStickDown") return GamepadAxisDirection::LeftStickDown;
    if (axis_name == "LeftStickLeft") return GamepadAxisDirection::LeftStickLeft;
    if (axis_name == "LeftStickRight") return GamepadAxisDirection::LeftStickRight;
    if (axis_name == "RightStickUp") return GamepadAxisDirection::RightStickUp;
    if (axis_name == "RightStickDown") return GamepadAxisDirection::RightStickDown;
    if (axis_name == "RightStickLeft") return GamepadAxisDirection::RightStickLeft;
    if (axis_name == "RightStickRight") return GamepadAxisDirection::RightStickRight;
    if (axis_name == "LeftTrigger") return GamepadAxisDirection::LeftTrigger;
    if (axis_name == "RightTrigger") return GamepadAxisDirection::RightTrigger;
    return std::nullopt;
}

[[nodiscard]] const char* mouseButtonToken(Uint32 button) {
    switch (button) {
        case SDL_BUTTON_LEFT: return "MouseLeft";
        case SDL_BUTTON_MIDDLE: return "MouseMiddle";
        case SDL_BUTTON_RIGHT: return "MouseRight";
        case SDL_BUTTON_X1: return "MouseX1";
        case SDL_BUTTON_X2: return "MouseX2";
        default: return "";
    }
}

[[nodiscard]] const char* gamepadButtonToken(SDL_GamepadButton button) {
    switch (button) {
        case SDL_GAMEPAD_BUTTON_SOUTH: return "GamepadSouth";
        case SDL_GAMEPAD_BUTTON_EAST: return "GamepadEast";
        case SDL_GAMEPAD_BUTTON_WEST: return "GamepadWest";
        case SDL_GAMEPAD_BUTTON_NORTH: return "GamepadNorth";
        case SDL_GAMEPAD_BUTTON_BACK: return "GamepadBack";
        case SDL_GAMEPAD_BUTTON_GUIDE: return "GamepadGuide";
        case SDL_GAMEPAD_BUTTON_START: return "GamepadStart";
        case SDL_GAMEPAD_BUTTON_LEFT_STICK: return "GamepadLeftStick";
        case SDL_GAMEPAD_BUTTON_RIGHT_STICK: return "GamepadRightStick";
        case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: return "GamepadLeftShoulder";
        case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: return "GamepadRightShoulder";
        case SDL_GAMEPAD_BUTTON_DPAD_UP: return "GamepadDpadUp";
        case SDL_GAMEPAD_BUTTON_DPAD_DOWN: return "GamepadDpadDown";
        case SDL_GAMEPAD_BUTTON_DPAD_LEFT: return "GamepadDpadLeft";
        case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: return "GamepadDpadRight";
        case SDL_GAMEPAD_BUTTON_INVALID:
        case SDL_GAMEPAD_BUTTON_MISC1:
        case SDL_GAMEPAD_BUTTON_MISC2:
        case SDL_GAMEPAD_BUTTON_MISC3:
        case SDL_GAMEPAD_BUTTON_MISC4:
        case SDL_GAMEPAD_BUTTON_MISC5:
        case SDL_GAMEPAD_BUTTON_MISC6:
        case SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1:
        case SDL_GAMEPAD_BUTTON_LEFT_PADDLE1:
        case SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2:
        case SDL_GAMEPAD_BUTTON_LEFT_PADDLE2:
        case SDL_GAMEPAD_BUTTON_TOUCHPAD:
        case SDL_GAMEPAD_BUTTON_COUNT:
            return "";
    }

    return "";
}

[[nodiscard]] const char* gamepadAxisDirectionToken(GamepadAxisDirection direction) {
    switch (direction) {
        case GamepadAxisDirection::LeftStickUp: return "LeftStickUp";
        case GamepadAxisDirection::LeftStickDown: return "LeftStickDown";
        case GamepadAxisDirection::LeftStickLeft: return "LeftStickLeft";
        case GamepadAxisDirection::LeftStickRight: return "LeftStickRight";
        case GamepadAxisDirection::RightStickUp: return "RightStickUp";
        case GamepadAxisDirection::RightStickDown: return "RightStickDown";
        case GamepadAxisDirection::RightStickLeft: return "RightStickLeft";
        case GamepadAxisDirection::RightStickRight: return "RightStickRight";
        case GamepadAxisDirection::LeftTrigger: return "LeftTrigger";
        case GamepadAxisDirection::RightTrigger: return "RightTrigger";
        case GamepadAxisDirection::Count: return "";
    }

    return "";
}

} // namespace

float normalizeStickAxis(const Sint16 value) {
    const float denominator = value < 0 ? 32768.0f : 32767.0f;
    return std::clamp(static_cast<float>(value) / denominator, -1.0f, 1.0f);
}

float normalizeTriggerAxis(const Sint16 value) {
    if (value <= 0) {
        return 0.0f;
    }
    return std::clamp(static_cast<float>(value) / 32767.0f, 0.0f, 1.0f);
}

std::optional<BindingDefinition> bindingDefinitionFromToken(const std::string_view token) {
    const SDL_Scancode scancode = scancodeFromString(token);
    if (scancode != SDL_SCANCODE_UNKNOWN) {
        return makeBindingDefinition(InputDevice::Keyboard, scancode, token);
    }

    const Uint32 mouse_button = mouseButtonFromString(token);
    if (mouse_button != 0) {
        return makeBindingDefinition(InputDevice::Mouse, mouse_button, token);
    }

    const SDL_GamepadButton gamepad_button = gamepadButtonFromString(token);
    if (gamepad_button != SDL_GAMEPAD_BUTTON_INVALID) {
        return makeBindingDefinition(InputDevice::Gamepad, gamepad_button, token);
    }

    const auto gamepad_axis_direction = gamepadAxisDirectionFromString(token);
    if (gamepad_axis_direction.has_value()) {
        return makeBindingDefinition(InputDevice::Gamepad, *gamepad_axis_direction, token);
    }

    return std::nullopt;
}

std::optional<BindingDefinition> bindingDefinitionFromEvent(const SDL_Event& event,
                                                            const SDL_JoystickID active_gamepad_id,
                                                            const float axis_press_threshold) {
    switch (event.type) {
        case SDL_EVENT_KEY_DOWN: {
            if (!event.key.down || event.key.repeat || event.key.scancode == SDL_SCANCODE_UNKNOWN) {
                return std::nullopt;
            }

            const char* name = SDL_GetScancodeName(event.key.scancode);
            if (name == nullptr || name[0] == '\0') {
                return std::nullopt;
            }

            return makeBindingDefinition(InputDevice::Keyboard, event.key.scancode, name);
        }
        case SDL_EVENT_MOUSE_BUTTON_DOWN: {
            if (!event.button.down) {
                return std::nullopt;
            }

            const char* token = mouseButtonToken(static_cast<Uint32>(event.button.button));
            if (token[0] == '\0') {
                return std::nullopt;
            }

            return makeBindingDefinition(InputDevice::Mouse, static_cast<Uint32>(event.button.button), token);
        }
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN: {
            if (!event.gbutton.down || event.gbutton.which != active_gamepad_id) {
                return std::nullopt;
            }

            const auto button = static_cast<SDL_GamepadButton>(event.gbutton.button);
            const char* token = gamepadButtonToken(button);
            if (token[0] == '\0') {
                return std::nullopt;
            }

            return makeBindingDefinition(InputDevice::Gamepad, button, token);
        }
        case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
            if (event.gaxis.which != active_gamepad_id) {
                return std::nullopt;
            }

            const auto axis = static_cast<SDL_GamepadAxis>(event.gaxis.axis);
            const bool is_trigger = (axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER || axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);
            const float normalized_value = is_trigger
                ? normalizeTriggerAxis(event.gaxis.value)
                : normalizeStickAxis(event.gaxis.value);

            std::optional<GamepadAxisDirection> direction;
            switch (axis) {
                case SDL_GAMEPAD_AXIS_LEFTX:
                    if (normalized_value > axis_press_threshold) direction = GamepadAxisDirection::LeftStickRight;
                    else if (-normalized_value > axis_press_threshold) direction = GamepadAxisDirection::LeftStickLeft;
                    break;
                case SDL_GAMEPAD_AXIS_LEFTY:
                    if (normalized_value > axis_press_threshold) direction = GamepadAxisDirection::LeftStickDown;
                    else if (-normalized_value > axis_press_threshold) direction = GamepadAxisDirection::LeftStickUp;
                    break;
                case SDL_GAMEPAD_AXIS_RIGHTX:
                    if (normalized_value > axis_press_threshold) direction = GamepadAxisDirection::RightStickRight;
                    else if (-normalized_value > axis_press_threshold) direction = GamepadAxisDirection::RightStickLeft;
                    break;
                case SDL_GAMEPAD_AXIS_RIGHTY:
                    if (normalized_value > axis_press_threshold) direction = GamepadAxisDirection::RightStickDown;
                    else if (-normalized_value > axis_press_threshold) direction = GamepadAxisDirection::RightStickUp;
                    break;
                case SDL_GAMEPAD_AXIS_LEFT_TRIGGER:
                    if (normalized_value > axis_press_threshold) direction = GamepadAxisDirection::LeftTrigger;
                    break;
                case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER:
                    if (normalized_value > axis_press_threshold) direction = GamepadAxisDirection::RightTrigger;
                    break;
                case SDL_GAMEPAD_AXIS_INVALID:
                case SDL_GAMEPAD_AXIS_COUNT:
                    break;
            }

            if (!direction.has_value()) {
                return std::nullopt;
            }

            const char* token = gamepadAxisDirectionToken(*direction);
            if (token[0] == '\0') {
                return std::nullopt;
            }

            return makeBindingDefinition(InputDevice::Gamepad, *direction, token);
        }
        default:
            return std::nullopt;
    }
}

} // namespace engine::input
