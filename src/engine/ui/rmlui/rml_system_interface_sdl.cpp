#include "engine/ui/rmlui/rml_system_interface_sdl.h"

#include <SDL3/SDL.h>

namespace engine::ui::rmlui {

namespace {

[[nodiscard]] engine::input::MouseCursorKind cursorKindFromRmlName(const Rml::String& cursor_name) {
    if (cursor_name.empty() || cursor_name == "arrow") {
        return engine::input::MouseCursorKind::Default;
    }
    if (cursor_name == "pointer") {
        return engine::input::MouseCursorKind::Pointer;
    }
    if (cursor_name == "move" || cursor_name.rfind("rmlui-scroll", 0) == 0) {
        return engine::input::MouseCursorKind::Grab;
    }
    if (cursor_name == "text") {
        return engine::input::MouseCursorKind::Text;
    }
    if (cursor_name == "resize") {
        return engine::input::MouseCursorKind::Resize;
    }
    if (cursor_name == "cross") {
        return engine::input::MouseCursorKind::Cross;
    }
    return engine::input::MouseCursorKind::Default;
}

} // namespace

RmlSystemInterfaceSdl::RmlSystemInterfaceSdl(engine::input::MouseCursorService* cursor_service)
    : cursor_service_(cursor_service) {
}

RmlSystemInterfaceSdl::~RmlSystemInterfaceSdl() = default;

void RmlSystemInterfaceSdl::setWindow(SDL_Window* window) noexcept {
    window_ = window;
}

double RmlSystemInterfaceSdl::GetElapsedTime() {
    static const Uint64 start = SDL_GetPerformanceCounter();
    static const double frequency = double(SDL_GetPerformanceFrequency());
    return double(SDL_GetPerformanceCounter() - start) / frequency;
}

void RmlSystemInterfaceSdl::SetMouseCursor(const Rml::String& cursor_name) {
    const auto kind = cursorKindFromRmlName(cursor_name);
    if (cursor_service_) {
        cursor_service_->setUiCursor(kind);
    }
}

void RmlSystemInterfaceSdl::SetClipboardText(const Rml::String& text) {
    SDL_SetClipboardText(text.c_str());
}

void RmlSystemInterfaceSdl::GetClipboardText(Rml::String& text) {
    char* raw_text = SDL_GetClipboardText();
    text = Rml::String(raw_text);
    SDL_free(raw_text);
}

void RmlSystemInterfaceSdl::ActivateKeyboard(Rml::Vector2f caret_position, float line_height) {
    if (!window_) {
        return;
    }

    const SDL_Rect rect = {int(caret_position.x), int(caret_position.y), 1, int(line_height)};
    SDL_SetTextInputArea(window_, &rect, 0);
    SDL_StartTextInput(window_);
}

void RmlSystemInterfaceSdl::DeactivateKeyboard() {
    if (window_) {
        SDL_StopTextInput(window_);
    }
}

} // namespace engine::ui::rmlui
