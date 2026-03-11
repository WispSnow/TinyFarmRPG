// NOLINTBEGIN
#include <gtest/gtest.h>

#include <SDL3/SDL.h>
#include <entt/signal/dispatcher.hpp>
#include <nlohmann/json.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "engine/core/game_state.h"
#include "engine/input/input_manager.h"
#include "engine/ui/ui_navigation_controller.h"

namespace engine::ui {
namespace {

[[nodiscard]] bool initSdlVideoWithDummyFallback(Uint32 flags) {
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
    return SDL_Init(flags);
}

struct NavigationProbe {
    int up_calls{0};
    int confirm_calls{0};

    void onUp() {
        ++up_calls;
    }

    void onConfirm() {
        ++confirm_calls;
    }
};

class UINavigationControllerTest : public ::testing::Test {
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
            GTEST_SKIP() << "SDL video/gamepad subsystem not available in this environment.";
        }

        const auto temp_dir = std::filesystem::temp_directory_path();
        const auto timestamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        config_path_ = temp_dir / ("ui_navigation_controller_test_" + std::to_string(timestamp) + ".json");

        dispatcher_ = std::make_unique<entt::dispatcher>();
        window_ = SDL_CreateWindow("UINavigationControllerTest", 640, 480, SDL_WINDOW_HIDDEN);
        ASSERT_NE(window_, nullptr);
        game_state_ = engine::core::GameState::create(window_);
        ASSERT_NE(game_state_, nullptr);
        drainEvents();
    }

    void TearDown() override {
        game_state_.reset();
        dispatcher_.reset();
        if (window_) {
            SDL_DestroyWindow(window_);
            window_ = nullptr;
        }

        std::error_code ec;
        std::filesystem::remove(config_path_, ec);
        drainEvents();
    }

    [[nodiscard]] std::unique_ptr<engine::input::InputManager> createManager() {
        nlohmann::json json;
        json["input_mappings"] = {
            {"menu_up", {"W"}},
            {"menu_confirm", {"Return"}},
        };

        std::ofstream config_file(config_path_);
        EXPECT_TRUE(config_file.is_open());
        config_file << json.dump(2);
        config_file.close();

        auto manager = engine::input::InputManager::create(dispatcher_.get(), game_state_.get(), config_path_.string());
        EXPECT_NE(manager, nullptr);
        return manager;
    }

    void drainEvents() {
        SDL_Event event{};
        while (SDL_PollEvent(&event)) {
        }
    }

    void pushKey(SDL_Scancode scancode, bool down) {
        SDL_Event event{};
        event.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
        event.key.scancode = scancode;
        event.key.down = down;
        event.key.repeat = false;
        ASSERT_EQ(SDL_PushEvent(&event), true);
    }
};

TEST_F(UINavigationControllerTest, EmitsNavigationSignalsOnlyOnPressed) {
    auto manager = createManager();
    ASSERT_NE(manager, nullptr);
    manager->pushContext(engine::input::InputContextId::Menu);

    UINavigationController controller(*manager);
    NavigationProbe probe{};
    controller.onNavigateUp().connect<&NavigationProbe::onUp>(&probe);

    pushKey(SDL_SCANCODE_W, true);
    manager->sampleInputEvents();
    manager->dispatchActionCallbacks();
    EXPECT_EQ(probe.up_calls, 1);

    manager->consumeTick();
    manager->dispatchActionCallbacks();
    EXPECT_EQ(probe.up_calls, 1);
}

TEST_F(UINavigationControllerTest, EmitsConfirmSignalOnPressed) {
    auto manager = createManager();
    ASSERT_NE(manager, nullptr);
    manager->pushContext(engine::input::InputContextId::Menu);

    UINavigationController controller(*manager);
    NavigationProbe probe{};
    controller.onConfirm().connect<&NavigationProbe::onConfirm>(&probe);

    pushKey(SDL_SCANCODE_RETURN, true);
    manager->sampleInputEvents();
    manager->dispatchActionCallbacks();

    EXPECT_EQ(probe.confirm_calls, 1);
}

} // namespace
} // namespace engine::ui
// NOLINTEND
