// NOLINTBEGIN
#include <gtest/gtest.h>

#include "engine/input/input_glyphs.h"

namespace engine::input {
namespace {

TEST(InputGlyphsTest, KeyboardPromptUsesStableTextAndIconId) {
    const auto binding = BindingDefinition{
        .token = "Return",
        .device = InputDevice::Keyboard,
        .physical_input = SDL_SCANCODE_RETURN,
        .prompt_icon_id = buildPromptIconId(InputDevice::Keyboard, PhysicalInput{SDL_SCANCODE_RETURN}, "Return"),
        .prompt_fallback_text = buildPromptFallbackText(InputDevice::Keyboard, PhysicalInput{SDL_SCANCODE_RETURN}, "Return"),
    };

    const auto prompt = makeActionPrompt(binding);
    EXPECT_EQ(prompt.device, InputDevice::Keyboard);
    EXPECT_EQ(prompt.token, "Return");
    EXPECT_EQ(prompt.icon_id, "key_return");
    EXPECT_EQ(prompt.fallback_text, "Return");
}

TEST(InputGlyphsTest, MouseAndGamepadPromptFallbacksAreReadable) {
    const auto mouse_icon = buildPromptIconId(InputDevice::Mouse, PhysicalInput{Uint32{SDL_BUTTON_LEFT}}, "MouseLeft");
    const auto mouse_text = buildPromptFallbackText(InputDevice::Mouse, PhysicalInput{Uint32{SDL_BUTTON_LEFT}}, "MouseLeft");
    EXPECT_EQ(mouse_icon, "mouse_left");
    EXPECT_EQ(mouse_text, "Mouse Left");

    const auto gamepad_icon = buildPromptIconId(InputDevice::Gamepad, PhysicalInput{SDL_GAMEPAD_BUTTON_SOUTH}, "GamepadSouth");
    const auto gamepad_text = buildPromptFallbackText(InputDevice::Gamepad, PhysicalInput{SDL_GAMEPAD_BUTTON_SOUTH}, "GamepadSouth");
    EXPECT_EQ(gamepad_icon.rfind("gamepad_", 0), 0U);
    EXPECT_FALSE(gamepad_text.empty());
}

} // namespace
} // namespace engine::input
// NOLINTEND
