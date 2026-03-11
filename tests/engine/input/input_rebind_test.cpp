// NOLINTBEGIN
#include <gtest/gtest.h>

#include <SDL3/SDL.h>
#include <entt/core/hashed_string.hpp>
#include <entt/signal/dispatcher.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string_view>

#include "engine/core/game_state.h"
#include "engine/input/input_manager.h"

namespace engine::input {
namespace {

using namespace entt::literals;

[[nodiscard]] bool initSdlVideoWithDummyFallback(Uint32 flags) {
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
    return SDL_Init(flags);
}

class InputRebindTest : public ::testing::Test {
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
        config_path_ = temp_dir / ("input_rebind_test_" + std::to_string(timestamp) + ".json");

        dispatcher_ = std::make_unique<entt::dispatcher>();
        window_ = SDL_CreateWindow("InputRebindTest", 640, 480, SDL_WINDOW_HIDDEN);
        ASSERT_NE(window_, nullptr);
        game_state_ = engine::core::GameState::create(window_);
        ASSERT_NE(game_state_, nullptr);

        drainEvents();
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
        drainEvents();
    }

    [[nodiscard]] std::unique_ptr<InputManager> createManager(std::string_view json) {
        std::ofstream config_file(config_path_);
        EXPECT_TRUE(config_file.is_open());
        config_file << json;
        config_file.close();

        auto manager = InputManager::create(dispatcher_.get(), game_state_.get(), config_path_.string());
        EXPECT_NE(manager, nullptr);
        return manager;
    }

    void drainEvents() {
        SDL_Event event{};
        while (SDL_PollEvent(&event)) {
        }
    }

    void pushKey(SDL_Scancode scancode, bool down, bool repeat = false) {
        SDL_Event event{};
        event.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
        event.key.scancode = scancode;
        event.key.down = down;
        event.key.repeat = repeat;
        ASSERT_EQ(SDL_PushEvent(&event), true);
    }
};

TEST_F(InputRebindTest, CaptureRebindsBindingAndSkipsRmlUiForwarding) {
    auto manager = createManager(R"({
        "input_mappings":{
            "move_left":["A"],
            "menu_confirm":["Return"]
        }
    })");
    ASSERT_NE(manager, nullptr);

    int rmlui_forward_count = 0;
    manager->setRmlUiEventForwarder([&](SDL_Event&) {
        ++rmlui_forward_count;
        return true;
    });

    ASSERT_TRUE(manager->beginRebindCapture("move_left"_hs, 0));
    pushKey(SDL_SCANCODE_Z, true);
    manager->sampleInputEvents();

    const auto bindings = manager->getActionBindings("move_left"_hs);
    ASSERT_EQ(bindings.size(), 1U);
    EXPECT_EQ(bindings[0].token, "Z");
    EXPECT_EQ(rmlui_forward_count, 0);

    std::ifstream config_file(config_path_);
    ASSERT_TRUE(config_file.is_open());
    const std::string content{std::istreambuf_iterator<char>(config_file), std::istreambuf_iterator<char>()};
    EXPECT_NE(content.find("\"Z\""), std::string::npos);
}

TEST_F(InputRebindTest, ConflictStaysPendingUntilConfirmed) {
    auto manager = createManager(R"({
        "input_mappings":{
            "move_left":["A"],
            "move_right":["D"]
        }
    })");
    ASSERT_NE(manager, nullptr);

    ASSERT_TRUE(manager->beginRebindCapture("move_left"_hs, 0));
    pushKey(SDL_SCANCODE_D, true);
    manager->sampleInputEvents();

    auto snapshot = manager->getDebugSnapshot();
    EXPECT_TRUE(snapshot.rebind.pending_conflict);
    ASSERT_EQ(manager->getActionBindings("move_left"_hs).size(), 1U);
    EXPECT_EQ(manager->getActionBindings("move_left"_hs)[0].token, "A");

    ASSERT_TRUE(manager->confirmPendingRebindConflict());
    EXPECT_EQ(manager->getActionBindings("move_left"_hs)[0].token, "D");
    EXPECT_TRUE(manager->getActionBindings("move_right"_hs).empty());
}

TEST_F(InputRebindTest, RepeatKeyDoesNotCommitBindingDuringCapture) {
    auto manager = createManager(R"({"input_mappings":{"move_left":["A"]}})");
    ASSERT_NE(manager, nullptr);

    ASSERT_TRUE(manager->beginRebindCapture("move_left"_hs, 0));
    pushKey(SDL_SCANCODE_X, true, true);
    manager->sampleInputEvents();

    const auto bindings = manager->getActionBindings("move_left"_hs);
    ASSERT_EQ(bindings.size(), 1U);
    EXPECT_EQ(bindings[0].token, "A");
    EXPECT_TRUE(manager->getDebugSnapshot().rebind.capture_active);
}

TEST_F(InputRebindTest, EscapeCancelsCaptureWithoutChangingBinding) {
    auto manager = createManager(R"({"input_mappings":{"move_left":["A"]}})");
    ASSERT_NE(manager, nullptr);

    ASSERT_TRUE(manager->beginRebindCapture("move_left"_hs, 0));
    pushKey(SDL_SCANCODE_ESCAPE, true);
    manager->sampleInputEvents();

    const auto bindings = manager->getActionBindings("move_left"_hs);
    ASSERT_EQ(bindings.size(), 1U);
    EXPECT_EQ(bindings[0].token, "A");
    EXPECT_FALSE(manager->getDebugSnapshot().rebind.capture_active);
}

} // namespace
} // namespace engine::input
// NOLINTEND
