#include "engine/input/input_event_routing.h"

#include "engine/input/input_context_registry.h"

#include <SDL3/SDL_events.h>

namespace engine::input {

bool shouldAlwaysPropagateAfterUi(const SDL_Event& event,
                                  const std::optional<InputContextId> current_context) {
    return event.type == SDL_EVENT_KEY_UP
        || event.type == SDL_EVENT_MOUSE_BUTTON_UP
        || event.type == SDL_EVENT_MOUSE_MOTION
        || (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN && isMenuLikeContext(current_context))
        || event.type == SDL_EVENT_GAMEPAD_BUTTON_UP
        || event.type == SDL_EVENT_GAMEPAD_AXIS_MOTION
        || event.type == SDL_EVENT_GAMEPAD_ADDED
        || event.type == SDL_EVENT_GAMEPAD_REMOVED
        || event.type == SDL_EVENT_GAMEPAD_REMAPPED;
}

bool isSystemEventDuringInputCapture(const SDL_Event& event) {
    switch (event.type) {
        case SDL_EVENT_WINDOW_RESIZED:
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        case SDL_EVENT_WINDOW_FOCUS_LOST:
        case SDL_EVENT_WINDOW_MINIMIZED:
        case SDL_EVENT_QUIT:
        case SDL_EVENT_GAMEPAD_ADDED:
        case SDL_EVENT_GAMEPAD_REMOVED:
        case SDL_EVENT_GAMEPAD_REMAPPED:
            return true;
        default:
            return false;
    }
}

bool shouldSuppressRmlUiKeyboardEvent(const SDL_Event& event,
                                      const std::optional<InputContextId> current_context,
                                      const std::unordered_set<SDL_Scancode>& suppressed_scancodes) {
    if ((event.type != SDL_EVENT_KEY_DOWN && event.type != SDL_EVENT_KEY_UP)
        || event.key.scancode == SDL_SCANCODE_TAB) {
        return false;
    }

    if (!isMenuLikeContext(current_context)) {
        return false;
    }

    return suppressed_scancodes.contains(event.key.scancode);
}

} // namespace engine::input
