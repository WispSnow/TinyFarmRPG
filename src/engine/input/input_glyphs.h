#pragma once

#include "engine/input/input_manager.h"

namespace engine::input {

[[nodiscard]] std::string buildPromptIconId(InputDevice device, const PhysicalInput& physical_input, std::string_view token);
[[nodiscard]] std::string buildPromptFallbackText(InputDevice device, const PhysicalInput& physical_input, std::string_view token);
[[nodiscard]] ActionPrompt makeActionPrompt(const BindingDefinition& binding);

} // namespace engine::input
