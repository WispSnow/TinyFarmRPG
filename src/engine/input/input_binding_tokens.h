#pragma once

#include "engine/input/input_manager.h"

#include <optional>
#include <string_view>

namespace engine::input {

[[nodiscard]] float normalizeStickAxis(Sint16 value);
[[nodiscard]] float normalizeTriggerAxis(Sint16 value);

[[nodiscard]] std::optional<BindingDefinition> bindingDefinitionFromToken(std::string_view token);
[[nodiscard]] std::optional<BindingDefinition> bindingDefinitionFromEvent(const SDL_Event& event,
                                                                          SDL_JoystickID active_gamepad_id,
                                                                          float axis_press_threshold);

} // namespace engine::input
