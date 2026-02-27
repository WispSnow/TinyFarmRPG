// NOLINTBEGIN
#include <gtest/gtest.h>

#include <SDL3/SDL.h>
#include <entt/core/hashed_string.hpp>
#include <entt/signal/dispatcher.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

#include "engine/core/game_state.h"
#include "engine/input/input_manager.h"

namespace engine::input {
namespace {

[[nodiscard]] bool initSdlVideoWithDummyFallback(Uint32 flags) {
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
    return SDL_Init(flags);
}

class InputManagerRmlUiRoutingTest : public ::testing::Test {
protected:
    SDL_Window* window_{nullptr};
    std::unique_ptr<engine::core::GameState> game_state_;
    std::unique_ptr<entt::dispatcher> dispatcher_;
    std::filesystem::path config_path_;
    static inline bool sdl_ready_{false};

    static void SetUpTestSuite() {
        sdl_ready_ = initSdlVideoWithDummyFallback(SDL_INIT_VIDEO);
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
        config_path_ = temp_dir / ("input_manager_rmlui_test_config_" + std::to_string(timestamp) + ".json");

        dispatcher_ = std::make_unique<entt::dispatcher>();
        window_ = SDL_CreateWindow("InputManagerRmlUiRoutingTest", 640, 480, SDL_WINDOW_HIDDEN);
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

TEST_F(InputManagerRmlUiRoutingTest, ConsumedPressDoesNotTriggerAction) {
    auto manager = InputManager::create(dispatcher_.get(), game_state_.get(), config_path_.string());
    ASSERT_NE(manager, nullptr);

    const entt::id_type action_id = entt::hashed_string{"move_left"};
    manager->setRmlUiEventForwarder([](SDL_Event& event) {
        if (event.type == SDL_EVENT_KEY_DOWN) {
            return false;
        }
        return true;
    });

    SDL_Event key_down{};
    key_down.type = SDL_EVENT_KEY_DOWN;
    key_down.key.scancode = SDL_SCANCODE_A;
    key_down.key.down = true;
    key_down.key.repeat = false;
    ASSERT_EQ(SDL_PushEvent(&key_down), true);

    manager->sampleInputEvents();

    EXPECT_FALSE(manager->isActionPressed(action_id));
    EXPECT_FALSE(manager->isActionDown(action_id));
}

TEST_F(InputManagerRmlUiRoutingTest, ConsumedReleaseStillClearsHeldAction) {
    auto manager = InputManager::create(dispatcher_.get(), game_state_.get(), config_path_.string());
    ASSERT_NE(manager, nullptr);

    const entt::id_type action_id = entt::hashed_string{"move_left"};
    manager->setRmlUiEventForwarder([](SDL_Event&) { return true; });

    SDL_Event key_down{};
    key_down.type = SDL_EVENT_KEY_DOWN;
    key_down.key.scancode = SDL_SCANCODE_A;
    key_down.key.down = true;
    key_down.key.repeat = false;
    ASSERT_EQ(SDL_PushEvent(&key_down), true);

    manager->sampleInputEvents();
    EXPECT_TRUE(manager->isActionPressed(action_id));

    manager->consumeTick();
    EXPECT_TRUE(manager->isActionDown(action_id));

    manager->setRmlUiEventForwarder([](SDL_Event&) { return false; });

    SDL_Event key_up{};
    key_up.type = SDL_EVENT_KEY_UP;
    key_up.key.scancode = SDL_SCANCODE_A;
    key_up.key.down = false;
    key_up.key.repeat = false;
    ASSERT_EQ(SDL_PushEvent(&key_up), true);

    manager->sampleInputEvents();

    EXPECT_TRUE(manager->isActionReleased(action_id));
    EXPECT_FALSE(manager->isActionDown(action_id));
}

#ifdef TF_ENABLE_DEBUG_UI
TEST_F(InputManagerRmlUiRoutingTest, ImGuiForwarderRunsOnlyWhenEventPropagates) {
    auto manager = InputManager::create(dispatcher_.get(), game_state_.get(), config_path_.string());
    ASSERT_NE(manager, nullptr);

    int imgui_forward_count = 0;
    manager->setImGuiEventForwarder([&](const SDL_Event&) {
        ++imgui_forward_count;
    });

    manager->setRmlUiEventForwarder([](SDL_Event&) { return false; });

    SDL_Event motion{};
    motion.type = SDL_EVENT_MOUSE_MOTION;
    motion.motion.x = 32;
    motion.motion.y = 48;
    ASSERT_EQ(SDL_PushEvent(&motion), true);

    manager->sampleInputEvents();
    EXPECT_EQ(imgui_forward_count, 0);

    manager->setRmlUiEventForwarder([](SDL_Event&) { return true; });

    SDL_Event motion2{};
    motion2.type = SDL_EVENT_MOUSE_MOTION;
    motion2.motion.x = 64;
    motion2.motion.y = 96;
    ASSERT_EQ(SDL_PushEvent(&motion2), true);

    manager->sampleInputEvents();
    EXPECT_GE(imgui_forward_count, 1);
}
#endif

} // namespace
} // namespace engine::input
// NOLINTEND
