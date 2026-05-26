#pragma once

#include "engine/input/mouse_cursor_service.h"

#include <RmlUi/Core/SystemInterface.h>
#include <RmlUi/Core/Types.h>

#include <SDL3/SDL.h>

namespace engine::ui::rmlui {

class RmlSystemInterfaceSdl final : public Rml::SystemInterface {
public:
    explicit RmlSystemInterfaceSdl(engine::input::MouseCursorService* cursor_service = nullptr);
    ~RmlSystemInterfaceSdl() override;

    RmlSystemInterfaceSdl(const RmlSystemInterfaceSdl&) = delete;
    RmlSystemInterfaceSdl& operator=(const RmlSystemInterfaceSdl&) = delete;
    RmlSystemInterfaceSdl(RmlSystemInterfaceSdl&&) = delete;
    RmlSystemInterfaceSdl& operator=(RmlSystemInterfaceSdl&&) = delete;

    void setWindow(SDL_Window* window) noexcept;

    double GetElapsedTime() override;
    void SetMouseCursor(const Rml::String& cursor_name) override;
    void SetClipboardText(const Rml::String& text) override;
    void GetClipboardText(Rml::String& text) override;
    void ActivateKeyboard(Rml::Vector2f caret_position, float line_height) override;
    void DeactivateKeyboard() override;

private:
    SDL_Window* window_{nullptr};
    engine::input::MouseCursorService* cursor_service_{nullptr};
};

} // namespace engine::ui::rmlui
