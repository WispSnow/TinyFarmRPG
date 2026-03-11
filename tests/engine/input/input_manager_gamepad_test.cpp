// NOLINTBEGIN
#include <gtest/gtest.h>

#include <SDL3/SDL.h>
#include <entt/core/hashed_string.hpp>
#include <entt/signal/dispatcher.hpp>
#include <nlohmann/json.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "engine/core/game_state.h"
#include "engine/input/input_manager.h"

namespace engine::input {
namespace {

[[nodiscard]] bool initSdlVideoWithDummyFallback(Uint32 flags) {
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
    return SDL_Init(flags);
}

struct VirtualGamepadHandle {
    SDL_JoystickID instance_id{0};
};

class InputManagerGamepadTest : public ::testing::Test {
protected:
    SDL_Window* window_{nullptr};
    std::unique_ptr<engine::core::GameState> game_state_;
    std::unique_ptr<entt::dispatcher> dispatcher_;
    std::filesystem::path config_path_;
    std::vector<VirtualGamepadHandle> attached_gamepads_;
    static inline bool sdl_ready_{false};

    static void SetUpTestSuite() {
        sdl_ready_ = initSdlVideoWithDummyFallback(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD);
    }

    static void TearDownTestSuite() {
        if (sdl_ready_) {
            SDL_Quit();
        }
    }

    void SetUp() override {
        if (!sdl_ready_) {
            GTEST_SKIP() << "SDL video/gamepad subsystem not available in this environment.";
        }

        const auto temp_dir = std::filesystem::temp_directory_path();
        const auto timestamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        config_path_ = temp_dir / ("input_manager_gamepad_test_config_" + std::to_string(timestamp) + ".json");
        dispatcher_ = std::make_unique<entt::dispatcher>();
        window_ = SDL_CreateWindow("InputManagerGamepadTest", 640, 480, SDL_WINDOW_HIDDEN);
        ASSERT_NE(window_, nullptr);
        game_state_ = engine::core::GameState::create(window_);
        ASSERT_NE(game_state_, nullptr);
        drainEvents();
    }

    void TearDown() override {
        for (auto& gamepad : attached_gamepads_) {
            detachVirtualGamepad(gamepad);
        }
        attached_gamepads_.clear();

        game_state_.reset();
        dispatcher_.reset();
        if (window_ != nullptr) {
            SDL_DestroyWindow(window_);
            window_ = nullptr;
        }

        std::error_code ec;
        std::filesystem::remove(config_path_, ec);
        drainEvents();
    }

    [[nodiscard]] std::unique_ptr<InputManager> createManager(const std::map<std::string, std::vector<std::string>>& mappings) {
        nlohmann::json json;
        json["input_mappings"] = mappings;

        std::ofstream config_file(config_path_);
        EXPECT_TRUE(config_file.is_open());
        config_file << json.dump(2);
        config_file.close();

        auto manager = InputManager::create(dispatcher_.get(), game_state_.get(), config_path_.string());
        EXPECT_NE(manager, nullptr);
        return manager;
    }

    [[nodiscard]] VirtualGamepadHandle attachVirtualGamepad(const char* name) {
        SDL_VirtualJoystickDesc desc{};
        SDL_INIT_INTERFACE(&desc);
        desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
        desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
        desc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
        desc.name = name;
        for (int i = 0; i < SDL_GAMEPAD_BUTTON_COUNT; ++i) {
            desc.button_mask |= (1u << i);
        }
        for (int i = 0; i < SDL_GAMEPAD_AXIS_COUNT; ++i) {
            desc.axis_mask |= (1u << i);
        }

        VirtualGamepadHandle handle;
        handle.instance_id = SDL_AttachVirtualJoystick(&desc);
        EXPECT_NE(handle.instance_id, 0);
        EXPECT_TRUE(SDL_IsGamepad(handle.instance_id));
        attached_gamepads_.push_back(handle);
        return handle;
    }

    void detachVirtualGamepad(VirtualGamepadHandle& handle) {
        if (handle.instance_id != 0) {
            EXPECT_TRUE(SDL_DetachVirtualJoystick(handle.instance_id));
            handle.instance_id = 0;
        }
    }

    void markDetached(SDL_JoystickID instance_id) {
        for (auto& handle : attached_gamepads_) {
            if (handle.instance_id == instance_id) {
                handle.instance_id = 0;
                return;
            }
        }
    }

    void drainEvents() {
        SDL_Event event{};
        while (SDL_PollEvent(&event)) {
        }
    }

    void pressKey(SDL_Scancode scancode, bool down) {
        SDL_Event event{};
        event.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
        event.key.scancode = scancode;
        event.key.down = down;
        event.key.repeat = false;
        ASSERT_EQ(SDL_PushEvent(&event), true);
    }

    void pushFocusLost() {
        SDL_Event event{};
        event.type = SDL_EVENT_WINDOW_FOCUS_LOST;
        event.window.windowID = SDL_GetWindowID(window_);
        ASSERT_EQ(SDL_PushEvent(&event), true);
    }

    void pushGamepadButtonEvent(SDL_JoystickID instance_id, SDL_GamepadButton button, bool down) {
        SDL_Event event{};
        event.type = down ? SDL_EVENT_GAMEPAD_BUTTON_DOWN : SDL_EVENT_GAMEPAD_BUTTON_UP;
        event.gbutton.which = instance_id;
        event.gbutton.button = static_cast<Uint8>(button);
        event.gbutton.down = down;
        ASSERT_EQ(SDL_PushEvent(&event), true);
    }

    void pushGamepadAxisEvent(SDL_JoystickID instance_id, SDL_GamepadAxis axis, Sint16 value) {
        SDL_Event event{};
        event.type = SDL_EVENT_GAMEPAD_AXIS_MOTION;
        event.gaxis.which = instance_id;
        event.gaxis.axis = static_cast<Uint8>(axis);
        event.gaxis.value = value;
        ASSERT_EQ(SDL_PushEvent(&event), true);
    }

    static constexpr Sint16 STICK_ACTIVE_VALUE = 25000;
    static constexpr Sint16 STICK_SOFT_VALUE = 18000;
    static constexpr Sint16 STICK_RELEASE_VALUE = 10000;
    static constexpr Sint16 TRIGGER_ACTIVE_VALUE = SDL_JOYSTICK_AXIS_MAX;
    static constexpr Sint16 TRIGGER_NEUTRAL_VALUE = 0;
};

TEST_F(InputManagerGamepadTest, GamepadButtonActionLifecycle) {
    auto manager = createManager({{"interact", {"GamepadSouth"}}});
    ASSERT_NE(manager, nullptr);
    const auto action_id = entt::hashed_string{"interact"}.value();

    auto gamepad = attachVirtualGamepad("LifecyclePad");
    manager->sampleInputEvents();

    pushGamepadButtonEvent(gamepad.instance_id, SDL_GAMEPAD_BUTTON_SOUTH, true);
    manager->sampleInputEvents();
    EXPECT_TRUE(manager->isActionPressed(action_id));
    EXPECT_TRUE(manager->isActionDown(action_id));

    manager->consumeTick();
    EXPECT_FALSE(manager->isActionPressed(action_id));
    EXPECT_TRUE(manager->isActionDown(action_id));

    pushGamepadButtonEvent(gamepad.instance_id, SDL_GAMEPAD_BUTTON_SOUTH, false);
    manager->sampleInputEvents();
    EXPECT_TRUE(manager->isActionReleased(action_id));
    EXPECT_FALSE(manager->isActionDown(action_id));

    manager->consumeTick();
    EXPECT_FALSE(manager->isActionReleased(action_id));
}

TEST_F(InputManagerGamepadTest, GamepadAxisDirectionPressAndHysteresis) {
    auto manager = createManager({{"move_right", {"LeftStickRight"}}});
    ASSERT_NE(manager, nullptr);
    const auto action_id = entt::hashed_string{"move_right"}.value();

    auto gamepad = attachVirtualGamepad("AxisPad");
    manager->sampleInputEvents();

    pushGamepadAxisEvent(gamepad.instance_id, SDL_GAMEPAD_AXIS_LEFTX, STICK_ACTIVE_VALUE);
    manager->sampleInputEvents();
    EXPECT_TRUE(manager->isActionPressed(action_id));

    manager->consumeTick();
    EXPECT_TRUE(manager->isActionDown(action_id));

    pushGamepadAxisEvent(gamepad.instance_id, SDL_GAMEPAD_AXIS_LEFTX, STICK_SOFT_VALUE);
    manager->sampleInputEvents();
    EXPECT_TRUE(manager->isActionDown(action_id));
    EXPECT_FALSE(manager->isActionReleased(action_id));

    pushGamepadAxisEvent(gamepad.instance_id, SDL_GAMEPAD_AXIS_LEFTX, STICK_RELEASE_VALUE);
    manager->sampleInputEvents();
    EXPECT_TRUE(manager->isActionReleased(action_id));
    EXPECT_FALSE(manager->isActionDown(action_id));
}

TEST_F(InputManagerGamepadTest, GamepadTriggerDirectionPress) {
    auto manager = createManager({{"trigger_test", {"LeftTrigger"}}});
    ASSERT_NE(manager, nullptr);
    const auto action_id = entt::hashed_string{"trigger_test"}.value();

    auto gamepad = attachVirtualGamepad("TriggerPad");
    manager->sampleInputEvents();

    pushGamepadAxisEvent(gamepad.instance_id, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, TRIGGER_NEUTRAL_VALUE);
    manager->sampleInputEvents();
    manager->consumeTick();

    pushGamepadAxisEvent(gamepad.instance_id, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, TRIGGER_ACTIVE_VALUE);
    manager->sampleInputEvents();
    EXPECT_TRUE(manager->isActionPressed(action_id));
    EXPECT_EQ(manager->getLastInputDevice(), InputDevice::Gamepad);
}

TEST_F(InputManagerGamepadTest, GamepadAndKeyboardSameAction) {
    auto manager = createManager({{"move_up", {"W", "GamepadDpadUp"}}});
    ASSERT_NE(manager, nullptr);
    const auto action_id = entt::hashed_string{"move_up"}.value();

    auto gamepad = attachVirtualGamepad("MixedPad");
    manager->sampleInputEvents();

    pressKey(SDL_SCANCODE_W, true);
    manager->sampleInputEvents();
    EXPECT_TRUE(manager->isActionPressed(action_id));
    manager->consumeTick();
    EXPECT_TRUE(manager->isActionDown(action_id));

    pushGamepadButtonEvent(gamepad.instance_id, SDL_GAMEPAD_BUTTON_DPAD_UP, true);
    manager->sampleInputEvents();
    EXPECT_TRUE(manager->isActionDown(action_id));
    EXPECT_FALSE(manager->isActionPressed(action_id));
    manager->consumeTick();

    pressKey(SDL_SCANCODE_W, false);
    manager->sampleInputEvents();
    EXPECT_TRUE(manager->isActionDown(action_id));
    EXPECT_FALSE(manager->isActionReleased(action_id));
    manager->consumeTick();

    pushGamepadButtonEvent(gamepad.instance_id, SDL_GAMEPAD_BUTTON_DPAD_UP, false);
    manager->sampleInputEvents();
    EXPECT_TRUE(manager->isActionReleased(action_id));
    EXPECT_FALSE(manager->isActionDown(action_id));
}

TEST_F(InputManagerGamepadTest, GamepadRemovalClearsState) {
    auto manager = createManager({{"interact", {"GamepadSouth"}}});
    ASSERT_NE(manager, nullptr);
    const auto action_id = entt::hashed_string{"interact"}.value();

    auto gamepad = attachVirtualGamepad("RemovalPad");
    manager->sampleInputEvents();

    pushGamepadButtonEvent(gamepad.instance_id, SDL_GAMEPAD_BUTTON_SOUTH, true);
    manager->sampleInputEvents();
    manager->consumeTick();
    EXPECT_TRUE(manager->isActionDown(action_id));

    const auto instance_id = gamepad.instance_id;
    detachVirtualGamepad(gamepad);
    markDetached(instance_id);
    manager->sampleInputEvents();

    EXPECT_FALSE(manager->isActionDown(action_id));
    EXPECT_FALSE(manager->isActionReleased(action_id));
    EXPECT_FALSE(manager->getGamepadDebugState().has_active_gamepad);
}

TEST_F(InputManagerGamepadTest, GamepadRemovalPreservesKeyboardContribution) {
    auto manager = createManager({{"move_up", {"W", "GamepadDpadUp"}}});
    ASSERT_NE(manager, nullptr);
    const auto action_id = entt::hashed_string{"move_up"}.value();

    auto gamepad = attachVirtualGamepad("RemovalPreservePad");
    manager->sampleInputEvents();

    pressKey(SDL_SCANCODE_W, true);
    manager->sampleInputEvents();
    manager->consumeTick();

    pushGamepadButtonEvent(gamepad.instance_id, SDL_GAMEPAD_BUTTON_DPAD_UP, true);
    manager->sampleInputEvents();
    manager->consumeTick();
    EXPECT_TRUE(manager->isActionDown(action_id));

    const auto instance_id = gamepad.instance_id;
    detachVirtualGamepad(gamepad);
    markDetached(instance_id);
    manager->sampleInputEvents();

    EXPECT_TRUE(manager->isActionDown(action_id));
    EXPECT_FALSE(manager->isActionReleased(action_id));
}

TEST_F(InputManagerGamepadTest, FocusLostClearsGamepadState) {
    auto manager = createManager({{"interact", {"GamepadSouth"}}});
    ASSERT_NE(manager, nullptr);
    const auto action_id = entt::hashed_string{"interact"}.value();

    auto gamepad = attachVirtualGamepad("FocusPad");
    manager->sampleInputEvents();

    pushGamepadButtonEvent(gamepad.instance_id, SDL_GAMEPAD_BUTTON_SOUTH, true);
    manager->sampleInputEvents();
    manager->consumeTick();
    EXPECT_TRUE(manager->isActionDown(action_id));

    pushFocusLost();
    manager->sampleInputEvents();

    EXPECT_FALSE(manager->isActionDown(action_id));
    EXPECT_FALSE(manager->isActionReleased(action_id));
    const auto debug = manager->getGamepadDebugState();
    EXPECT_FALSE(debug.button_states[static_cast<std::size_t>(SDL_GAMEPAD_BUTTON_SOUTH)]);
}

TEST_F(InputManagerGamepadTest, LastInputDeviceTracking) {
    auto manager = createManager({
        {"move_left", {"A"}},
        {"interact", {"GamepadSouth"}}
    });
    ASSERT_NE(manager, nullptr);

    auto gamepad = attachVirtualGamepad("LastDevicePad");
    manager->sampleInputEvents();

    pressKey(SDL_SCANCODE_A, true);
    manager->sampleInputEvents();
    EXPECT_EQ(manager->getLastInputDevice(), InputDevice::Keyboard);
    manager->consumeTick();

    pushGamepadButtonEvent(gamepad.instance_id, SDL_GAMEPAD_BUTTON_SOUTH, true);
    manager->sampleInputEvents();
    EXPECT_EQ(manager->getLastInputDevice(), InputDevice::Gamepad);
}

TEST_F(InputManagerGamepadTest, NewestConnectedGamepadBecomesActive) {
    auto manager = createManager({{"interact", {"GamepadSouth"}}});
    ASSERT_NE(manager, nullptr);
    const auto action_id = entt::hashed_string{"interact"}.value();

    auto gamepad_a = attachVirtualGamepad("PadA");
    manager->sampleInputEvents();
    ASSERT_EQ(manager->getGamepadDebugState().active_gamepad_id, gamepad_a.instance_id);

    pushGamepadButtonEvent(gamepad_a.instance_id, SDL_GAMEPAD_BUTTON_SOUTH, true);
    manager->sampleInputEvents();
    EXPECT_TRUE(manager->isActionPressed(action_id));
    manager->consumeTick();
    pushGamepadButtonEvent(gamepad_a.instance_id, SDL_GAMEPAD_BUTTON_SOUTH, false);
    manager->sampleInputEvents();
    manager->consumeTick();

    auto gamepad_b = attachVirtualGamepad("PadB");
    manager->sampleInputEvents();
    ASSERT_EQ(manager->getGamepadDebugState().active_gamepad_id, gamepad_b.instance_id);

    pushGamepadButtonEvent(gamepad_a.instance_id, SDL_GAMEPAD_BUTTON_SOUTH, true);
    manager->sampleInputEvents();
    EXPECT_FALSE(manager->isActionPressed(action_id));
    EXPECT_FALSE(manager->isActionDown(action_id));

    pushGamepadButtonEvent(gamepad_b.instance_id, SDL_GAMEPAD_BUTTON_SOUTH, true);
    manager->sampleInputEvents();
    EXPECT_TRUE(manager->isActionPressed(action_id));
}

TEST_F(InputManagerGamepadTest, GamepadButtonStringMappings) {
    const std::vector<std::pair<std::string, SDL_GamepadButton>> mappings = {
        {"GamepadSouth", SDL_GAMEPAD_BUTTON_SOUTH},
        {"GamepadEast", SDL_GAMEPAD_BUTTON_EAST},
        {"GamepadWest", SDL_GAMEPAD_BUTTON_WEST},
        {"GamepadNorth", SDL_GAMEPAD_BUTTON_NORTH},
        {"GamepadStart", SDL_GAMEPAD_BUTTON_START},
        {"GamepadBack", SDL_GAMEPAD_BUTTON_BACK},
        {"GamepadGuide", SDL_GAMEPAD_BUTTON_GUIDE},
        {"GamepadLeftStick", SDL_GAMEPAD_BUTTON_LEFT_STICK},
        {"GamepadRightStick", SDL_GAMEPAD_BUTTON_RIGHT_STICK},
        {"GamepadLeftShoulder", SDL_GAMEPAD_BUTTON_LEFT_SHOULDER},
        {"GamepadRightShoulder", SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER},
        {"GamepadDpadUp", SDL_GAMEPAD_BUTTON_DPAD_UP},
        {"GamepadDpadDown", SDL_GAMEPAD_BUTTON_DPAD_DOWN},
        {"GamepadDpadLeft", SDL_GAMEPAD_BUTTON_DPAD_LEFT},
        {"GamepadDpadRight", SDL_GAMEPAD_BUTTON_DPAD_RIGHT},
    };

    std::map<std::string, std::vector<std::string>> config;
    for (std::size_t i = 0; i < mappings.size(); ++i) {
        config.emplace("button_action_" + std::to_string(i), std::vector<std::string>{mappings[i].first});
    }

    auto manager = createManager(config);
    ASSERT_NE(manager, nullptr);
    auto gamepad = attachVirtualGamepad("ButtonMapPad");
    manager->sampleInputEvents();

    for (std::size_t i = 0; i < mappings.size(); ++i) {
        const auto action_id = entt::hashed_string(("button_action_" + std::to_string(i)).c_str()).value();
        pushGamepadButtonEvent(gamepad.instance_id, mappings[i].second, true);
        manager->sampleInputEvents();
        EXPECT_TRUE(manager->isActionPressed(action_id)) << mappings[i].first;
        manager->consumeTick();

        pushGamepadButtonEvent(gamepad.instance_id, mappings[i].second, false);
        manager->sampleInputEvents();
        EXPECT_TRUE(manager->isActionReleased(action_id)) << mappings[i].first;
        manager->consumeTick();
    }
}

TEST_F(InputManagerGamepadTest, GamepadAxisDirectionStringMappings) {
    struct AxisCase {
        const char* binding;
        SDL_GamepadAxis axis;
        Sint16 active_value;
        Sint16 neutral_value;
    };

    const std::vector<AxisCase> mappings = {
        {"LeftStickUp", SDL_GAMEPAD_AXIS_LEFTY, -STICK_ACTIVE_VALUE, 0},
        {"LeftStickDown", SDL_GAMEPAD_AXIS_LEFTY, STICK_ACTIVE_VALUE, 0},
        {"LeftStickLeft", SDL_GAMEPAD_AXIS_LEFTX, -STICK_ACTIVE_VALUE, 0},
        {"LeftStickRight", SDL_GAMEPAD_AXIS_LEFTX, STICK_ACTIVE_VALUE, 0},
        {"RightStickUp", SDL_GAMEPAD_AXIS_RIGHTY, -STICK_ACTIVE_VALUE, 0},
        {"RightStickDown", SDL_GAMEPAD_AXIS_RIGHTY, STICK_ACTIVE_VALUE, 0},
        {"RightStickLeft", SDL_GAMEPAD_AXIS_RIGHTX, -STICK_ACTIVE_VALUE, 0},
        {"RightStickRight", SDL_GAMEPAD_AXIS_RIGHTX, STICK_ACTIVE_VALUE, 0},
        {"LeftTrigger", SDL_GAMEPAD_AXIS_LEFT_TRIGGER, TRIGGER_ACTIVE_VALUE, TRIGGER_NEUTRAL_VALUE},
        {"RightTrigger", SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, TRIGGER_ACTIVE_VALUE, TRIGGER_NEUTRAL_VALUE},
    };

    std::map<std::string, std::vector<std::string>> config;
    for (std::size_t i = 0; i < mappings.size(); ++i) {
        config.emplace("axis_action_" + std::to_string(i), std::vector<std::string>{mappings[i].binding});
    }

    auto manager = createManager(config);
    ASSERT_NE(manager, nullptr);
    auto gamepad = attachVirtualGamepad("AxisMapPad");
    manager->sampleInputEvents();

    for (std::size_t i = 0; i < mappings.size(); ++i) {
        const auto action_id = entt::hashed_string(("axis_action_" + std::to_string(i)).c_str()).value();
        pushGamepadAxisEvent(gamepad.instance_id, mappings[i].axis, mappings[i].neutral_value);
        manager->sampleInputEvents();
        manager->consumeTick();

        pushGamepadAxisEvent(gamepad.instance_id, mappings[i].axis, mappings[i].active_value);
        manager->sampleInputEvents();
        EXPECT_TRUE(manager->isActionPressed(action_id)) << mappings[i].binding;
        manager->consumeTick();

        pushGamepadAxisEvent(gamepad.instance_id, mappings[i].axis, mappings[i].neutral_value);
        manager->sampleInputEvents();
        EXPECT_TRUE(manager->isActionReleased(action_id)) << mappings[i].binding;
        manager->consumeTick();
    }
}

} // namespace
} // namespace engine::input
// NOLINTEND
