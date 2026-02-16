// NOLINTBEGIN
#include <gtest/gtest.h>

#include <SDL3/SDL.h>
#include <entt/core/hashed_string.hpp>
#include <entt/signal/dispatcher.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <vector>

#include "engine/core/game_state.h"
#include "engine/input/input_manager.h"

namespace engine::input {
namespace {

class InputManagerTest : public ::testing::Test {
protected:
    SDL_Window* window_{nullptr};
    std::unique_ptr<engine::core::GameState> game_state_;
    std::unique_ptr<entt::dispatcher> dispatcher_;
    std::filesystem::path config_path_;
    static inline bool sdl_ready_{false};

    static void SetUpTestSuite() {
        sdl_ready_ = SDL_Init(SDL_INIT_VIDEO);
    }

    static void TearDownTestSuite() {
        if (sdl_ready_) {
            SDL_Quit();
        }
    }

    void SetUp() override {
        if (!sdl_ready_) {
            GTEST_SKIP() << "SDL video subsystem not available in this environment.";
        }
        const auto temp_dir = std::filesystem::temp_directory_path();
        const auto timestamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        config_path_ = temp_dir / ("input_manager_test_config_" + std::to_string(timestamp) + ".json");
        dispatcher_ = std::make_unique<entt::dispatcher>();
	        window_ = SDL_CreateWindow("InputManagerTest", 640, 480, SDL_WINDOW_HIDDEN);
	        ASSERT_NE(window_, nullptr);

	        game_state_ = engine::core::GameState::create(window_);
	        ASSERT_NE(game_state_, nullptr);

        std::ofstream config_file(config_path_);
        ASSERT_TRUE(config_file.is_open());
        config_file << R"({"input_mappings":{"move_left":["A"]}})";
        config_file.close();

        SDL_Event event{};
        while (SDL_PollEvent(&event)) {
        }
    }

    void TearDown() override {
        game_state_.reset();
        dispatcher_.reset();
        if (window_ != nullptr) {
            SDL_DestroyWindow(window_);
            window_ = nullptr;
        }

        std::error_code error_code;
        std::filesystem::remove(config_path_, error_code);

        SDL_Event event{};
        while (SDL_PollEvent(&event)) {
        }
    }
	};

	TEST_F(InputManagerTest, KeyboardActionLifecycle) {
	    auto manager = InputManager::create(dispatcher_.get(), game_state_.get(), config_path_.string());
	    ASSERT_NE(manager, nullptr);
	    const entt::id_type action_id = entt::hashed_string{"move_left"};

	    EXPECT_FALSE(manager->isActionDown(action_id));
	    EXPECT_FALSE(manager->isActionPressed(action_id));

    SDL_Event key_down{};
    key_down.type = SDL_EVENT_KEY_DOWN;
    key_down.key.scancode = SDL_SCANCODE_A;
    key_down.key.down = true;
    key_down.key.repeat = false;
    ASSERT_EQ(SDL_PushEvent(&key_down), true);

	    manager->update();

	    EXPECT_TRUE(manager->isActionPressed(action_id));
	    EXPECT_TRUE(manager->isActionDown(action_id));

	    manager->update();

	    EXPECT_FALSE(manager->isActionPressed(action_id));
	    EXPECT_TRUE(manager->isActionDown(action_id));

    SDL_Event key_up{};
    key_up.type = SDL_EVENT_KEY_UP;
    key_up.key.scancode = SDL_SCANCODE_A;
    key_up.key.down = false;
    key_up.key.repeat = false;
    ASSERT_EQ(SDL_PushEvent(&key_up), true);

	    manager->update();

	    EXPECT_TRUE(manager->isActionReleased(action_id));
	    EXPECT_FALSE(manager->isActionDown(action_id));

	    manager->update();

	    EXPECT_FALSE(manager->isActionReleased(action_id));
	    EXPECT_FALSE(manager->isActionDown(action_id));
	}

	TEST_F(InputManagerTest, DefaultMouseActionReceivesEvents) {
	    auto manager = InputManager::create(dispatcher_.get(), game_state_.get(), config_path_.string());
	    ASSERT_NE(manager, nullptr);
	    const entt::id_type mouse_action_id = entt::hashed_string{"mouse_left"};

    SDL_Event button_down{};
    button_down.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    button_down.button.button = SDL_BUTTON_LEFT;
    button_down.button.down = true;
    button_down.button.x = 120;
    button_down.button.y = 80;
    ASSERT_EQ(SDL_PushEvent(&button_down), true);

	    manager->update();

	    EXPECT_TRUE(manager->isActionPressed(mouse_action_id));
	    EXPECT_TRUE(manager->isActionDown(mouse_action_id));

	    const auto mouse_position = manager->getMousePosition();
	    EXPECT_FLOAT_EQ(mouse_position.x, 120.0F);
	    EXPECT_FLOAT_EQ(mouse_position.y, 80.0F);

    SDL_Event button_up{};
    button_up.type = SDL_EVENT_MOUSE_BUTTON_UP;
    button_up.button.button = SDL_BUTTON_LEFT;
    button_up.button.down = false;
    ASSERT_EQ(SDL_PushEvent(&button_up), true);

	    manager->update();

	    EXPECT_TRUE(manager->isActionReleased(mouse_action_id));
	    EXPECT_FALSE(manager->isActionDown(mouse_action_id));
	}

	TEST_F(InputManagerTest, ImGuiForwarderReceivesQueuedEvents) {
	    auto manager = InputManager::create(dispatcher_.get(), game_state_.get(), config_path_.string());
	    ASSERT_NE(manager, nullptr);
	    int callback_count = 0;
	    bool received_motion = false;
	    manager->setImGuiEventForwarder([&](const SDL_Event& event) {
	        ++callback_count;
	        if (event.type == SDL_EVENT_MOUSE_MOTION) {
	            received_motion = true;
	        }
	    });

    SDL_Event motion{};
    motion.type = SDL_EVENT_MOUSE_MOTION;
    motion.motion.x = 256;
    motion.motion.y = 144;
    ASSERT_EQ(SDL_PushEvent(&motion), true);

	    manager->update();

	    EXPECT_GE(callback_count, 1);
	    EXPECT_TRUE(received_motion);
	    const auto logical_position = manager->getLogicalMousePosition();
	    EXPECT_FLOAT_EQ(logical_position.x, 256.0F);
	    EXPECT_FLOAT_EQ(logical_position.y, 144.0F);
	}

} // namespace
} // namespace engine::input
// NOLINTEND
