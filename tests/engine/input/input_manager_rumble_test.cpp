// NOLINTBEGIN
#include <gtest/gtest.h>

#include <SDL3/SDL.h>
#include <entt/signal/dispatcher.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>

#include "engine/core/game_state.h"
#include "engine/input/input_manager.h"

namespace engine::input {
namespace {

[[nodiscard]] bool initSdlVideoWithDummyFallback(Uint32 flags) {
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
    return SDL_Init(flags);
}

class InputManagerRumbleTest : public ::testing::Test {
protected:
    SDL_Window* window_{nullptr};
    std::unique_ptr<engine::core::GameState> game_state_{};
    std::unique_ptr<entt::dispatcher> dispatcher_{};
    std::filesystem::path config_path_{};
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
            GTEST_SKIP() << "SDL video subsystem not available in this environment.";
        }

        const auto temp_dir = std::filesystem::temp_directory_path();
        const auto timestamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        config_path_ = temp_dir / ("input_manager_rumble_test_" + std::to_string(timestamp) + ".json");

        dispatcher_ = std::make_unique<entt::dispatcher>();
        window_ = SDL_CreateWindow("InputManagerRumbleTest", 640, 480, SDL_WINDOW_HIDDEN);
        ASSERT_NE(window_, nullptr);
        game_state_ = engine::core::GameState::create(window_);
        ASSERT_NE(game_state_, nullptr);

        std::ofstream config_file(config_path_);
        ASSERT_TRUE(config_file.is_open());
        config_file << R"({"input_mappings":{"move_left":["A"]}})";
        config_file.close();
    }

    void TearDown() override {
        game_state_.reset();
        dispatcher_.reset();
        if (window_ != nullptr) {
            SDL_DestroyWindow(window_);
            window_ = nullptr;
        }

        std::error_code ec;
        std::filesystem::remove(config_path_, ec);
    }
};

TEST_F(InputManagerRumbleTest, NoActiveGamepadStillRecordsLastRequest) {
    auto manager = InputManager::create(dispatcher_.get(), game_state_.get(), config_path_.string());
    ASSERT_NE(manager, nullptr);

    EXPECT_FALSE(manager->rumble(0.5f, 120));

    const auto snapshot = manager->getDebugSnapshot();
    EXPECT_TRUE(snapshot.rumble.has_last_request);
    EXPECT_FLOAT_EQ(snapshot.rumble.last_intensity, 0.5f);
    EXPECT_EQ(snapshot.rumble.last_duration_ms, 120U);
    EXPECT_FALSE(snapshot.rumble.last_request_succeeded);
    EXPECT_FALSE(snapshot.rumble.active);
}

} // namespace
} // namespace engine::input
// NOLINTEND
