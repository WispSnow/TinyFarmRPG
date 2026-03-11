#include "engine/input/input_glyphs.h"

#include <algorithm>
#include <cctype>
#include <type_traits>

namespace engine::input {

namespace {

[[nodiscard]] std::string sanitizeToken(std::string_view token) {
    std::string result;
    result.reserve(token.size());

    for (const unsigned char ch : token) {
        if (std::isalnum(ch) != 0) {
            result.push_back(static_cast<char>(std::tolower(ch)));
        } else {
            result.push_back('_');
        }
    }

    return result;
}

[[nodiscard]] const char* mouseTokenName(Uint32 button) {
    switch (button) {
        case SDL_BUTTON_LEFT: return "Mouse Left";
        case SDL_BUTTON_RIGHT: return "Mouse Right";
        case SDL_BUTTON_MIDDLE: return "Mouse Middle";
        case SDL_BUTTON_X1: return "Mouse X1";
        case SDL_BUTTON_X2: return "Mouse X2";
        default: return "Mouse Button";
    }
}

[[nodiscard]] const char* axisDirectionName(GamepadAxisDirection direction) {
    switch (direction) {
        case GamepadAxisDirection::LeftStickUp: return "Left Stick Up";
        case GamepadAxisDirection::LeftStickDown: return "Left Stick Down";
        case GamepadAxisDirection::LeftStickLeft: return "Left Stick Left";
        case GamepadAxisDirection::LeftStickRight: return "Left Stick Right";
        case GamepadAxisDirection::RightStickUp: return "Right Stick Up";
        case GamepadAxisDirection::RightStickDown: return "Right Stick Down";
        case GamepadAxisDirection::RightStickLeft: return "Right Stick Left";
        case GamepadAxisDirection::RightStickRight: return "Right Stick Right";
        case GamepadAxisDirection::LeftTrigger: return "Left Trigger";
        case GamepadAxisDirection::RightTrigger: return "Right Trigger";
        case GamepadAxisDirection::Count: return "Unknown Axis";
    }

    return "Unknown Axis";
}

} // namespace

std::string buildPromptIconId(InputDevice device, const PhysicalInput& physical_input, std::string_view token) {
    return std::visit([&](const auto& value) -> std::string {
        using ValueT = std::decay_t<decltype(value)>;

        if constexpr (std::is_same_v<ValueT, SDL_Scancode>) {
            const char* name = SDL_GetScancodeName(value);
            return "key_" + sanitizeToken(name != nullptr && name[0] != '\0' ? std::string_view{name} : token);
        } else if constexpr (std::is_same_v<ValueT, Uint32>) {
            (void)device;
            std::string sanitized = sanitizeToken(mouseTokenName(value));
            constexpr std::string_view mouse_prefix = "mouse_";
            if (sanitized.rfind(mouse_prefix, 0) == 0) {
                sanitized.erase(0, mouse_prefix.size());
            }
            return "mouse_" + sanitized;
        } else if constexpr (std::is_same_v<ValueT, SDL_GamepadButton>) {
            const char* name = SDL_GetGamepadStringForButton(value);
            return "gamepad_" + sanitizeToken(name != nullptr && name[0] != '\0' ? std::string_view{name} : token);
        } else {
            return "gamepad_" + sanitizeToken(axisDirectionName(value));
        }
    }, physical_input);
}

std::string buildPromptFallbackText(InputDevice device, const PhysicalInput& physical_input, std::string_view token) {
    return std::visit([&](const auto& value) -> std::string {
        using ValueT = std::decay_t<decltype(value)>;

        if constexpr (std::is_same_v<ValueT, SDL_Scancode>) {
            const char* name = SDL_GetScancodeName(value);
            if (name != nullptr && name[0] != '\0') {
                return name;
            }
        } else if constexpr (std::is_same_v<ValueT, Uint32>) {
            return mouseTokenName(value);
        } else if constexpr (std::is_same_v<ValueT, SDL_GamepadButton>) {
            if (const char* name = SDL_GetGamepadStringForButton(value); name != nullptr && name[0] != '\0') {
                return name;
            }
        } else {
            return axisDirectionName(value);
        }

        (void)device;
        return std::string(token);
    }, physical_input);
}

ActionPrompt makeActionPrompt(const BindingDefinition& binding) {
    return ActionPrompt{
        .device = binding.device,
        .token = binding.token,
        .icon_id = binding.prompt_icon_id,
        .fallback_text = binding.prompt_fallback_text,
    };
}

} // namespace engine::input
