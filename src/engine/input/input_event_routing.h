#pragma once

#include "engine/input/input_manager.h"

#include <optional>
#include <unordered_set>

namespace engine::input {

[[nodiscard]] bool shouldAlwaysPropagateAfterUi(const SDL_Event& event);
[[nodiscard]] bool isSystemEventDuringInputCapture(const SDL_Event& event);
[[nodiscard]] bool shouldSuppressRmlUiKeyboardEvent(const SDL_Event& event,
                                                    std::optional<InputContextId> current_context,
                                                    const std::unordered_set<SDL_Scancode>& suppressed_scancodes);

} // namespace engine::input
