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
#include <optional>
#include <string>
#include <vector>

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

struct CallbackListener {
    int calls{0};
    bool consume{true};

    bool onAction() {
        ++calls;
        return consume;
    }
};

class InputContextTest : public ::testing::Test {
protected:
    SDL_Window* window_{nullptr};
    std::unique_ptr<engine::core::GameState> game_state_;
    std::unique_ptr<entt::dispatcher> dispatcher_;
    std::filesystem::path config_path_;
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
        config_path_ = temp_dir / ("input_context_test_config_" + std::to_string(timestamp) + ".json");
        dispatcher_ = std::make_unique<entt::dispatcher>();
        window_ = SDL_CreateWindow("InputContextTest", 640, 480, SDL_WINDOW_HIDDEN);
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

    void pushFocusLost() {
        SDL_Event event{};
        event.type = SDL_EVENT_WINDOW_FOCUS_LOST;
        event.window.windowID = SDL_GetWindowID(window_);
        ASSERT_EQ(SDL_PushEvent(&event), true);
    }
};

TEST_F(InputContextTest, EmptyStackUsesLegacyBehavior) {
    auto manager = createManager({{"move_left", {"A"}}});
    ASSERT_NE(manager, nullptr);
    const auto move_left = "move_left"_hs;

    EXPECT_FALSE(manager->currentContext().has_value());

    pushKey(SDL_SCANCODE_A, true);
    manager->sampleInputEvents();

    EXPECT_TRUE(manager->isActionPressed(move_left));
    EXPECT_TRUE(manager->isActionDown(move_left));
}

TEST_F(InputContextTest, MenuContextFiltersSharedPhysicalBindingPerAction) {
    auto manager = createManager({
        {"move_up", {"W"}},
        {"menu_up", {"W"}},
    });
    ASSERT_NE(manager, nullptr);

    manager->pushContext(InputContextId::Menu);

    pushKey(SDL_SCANCODE_W, true);
    manager->sampleInputEvents();

    EXPECT_EQ(manager->currentContext(), std::optional<InputContextId>{InputContextId::Menu});
    EXPECT_FALSE(manager->isActionPressed("move_up"_hs));
    EXPECT_FALSE(manager->isActionDown("move_up"_hs));
    EXPECT_TRUE(manager->isActionPressed("menu_up"_hs));
    EXPECT_TRUE(manager->isActionDown("menu_up"_hs));
}

TEST_F(InputContextTest, ContextSwitchClearsActiveActionsAndPhysicalCaches) {
    auto manager = createManager({{"move_left", {"A"}}});
    ASSERT_NE(manager, nullptr);
    const auto move_left = "move_left"_hs;

    manager->pushContext(InputContextId::Gameplay);
    pushKey(SDL_SCANCODE_A, true);
    manager->sampleInputEvents();
    EXPECT_TRUE(manager->isActionPressed(move_left));

    manager->consumeTick();
    EXPECT_TRUE(manager->isActionDown(move_left));

    manager->pushContext(InputContextId::Dialogue);
    EXPECT_FALSE(manager->isActionDown(move_left));

    pushKey(SDL_SCANCODE_A, false);
    manager->sampleInputEvents();
    EXPECT_FALSE(manager->isActionReleased(move_left));

    manager->popContext();
    pushKey(SDL_SCANCODE_A, true);
    manager->sampleInputEvents();
    EXPECT_TRUE(manager->isActionPressed(move_left));
}

TEST_F(InputContextTest, ContextStackMultipleLevelsRestoresPreviousFilters) {
    auto manager = createManager({
        {"move_left", {"A"}},
        {"menu_cancel", {"Escape"}},
    });
    ASSERT_NE(manager, nullptr);

    manager->pushContext(InputContextId::Gameplay);
    manager->pushContext(InputContextId::Menu);
    manager->pushContext(InputContextId::Dialogue);

    pushKey(SDL_SCANCODE_A, true);
    manager->sampleInputEvents();
    EXPECT_FALSE(manager->isActionPressed("move_left"_hs));

    manager->popContext();
    pushKey(SDL_SCANCODE_ESCAPE, true);
    manager->sampleInputEvents();
    EXPECT_TRUE(manager->isActionPressed("menu_cancel"_hs));

    manager->popContext();
    pushKey(SDL_SCANCODE_A, true);
    manager->sampleInputEvents();
    EXPECT_TRUE(manager->isActionPressed("move_left"_hs));
}

TEST_F(InputContextTest, FocusLostBypassesContextFilter) {
    auto manager = createManager({{"menu_cancel", {"Escape"}}});
    ASSERT_NE(manager, nullptr);
    const auto menu_cancel = "menu_cancel"_hs;

    manager->pushContext(InputContextId::Menu);
    pushKey(SDL_SCANCODE_ESCAPE, true);
    manager->sampleInputEvents();
    manager->consumeTick();
    EXPECT_TRUE(manager->isActionDown(menu_cancel));

    pushFocusLost();
    manager->sampleInputEvents();

    EXPECT_EQ(manager->currentContext(), std::optional<InputContextId>{InputContextId::Menu});
    EXPECT_FALSE(manager->isActionPressed(menu_cancel));
    EXPECT_FALSE(manager->isActionDown(menu_cancel));
    EXPECT_FALSE(manager->isActionReleased(menu_cancel));
}

TEST_F(InputContextTest, PopContextOnEmptyStackIsNoOp) {
    auto manager = createManager({{"move_left", {"A"}}});
    ASSERT_NE(manager, nullptr);

    manager->popContext();
    EXPECT_FALSE(manager->currentContext().has_value());

    pushKey(SDL_SCANCODE_A, true);
    manager->sampleInputEvents();
    EXPECT_TRUE(manager->isActionPressed("move_left"_hs));
}

TEST_F(InputContextTest, StackedMenuCallbacksPreferTopListenerAndRestoreAfterPop) {
    auto manager = createManager({{"menu_cancel", {"Escape"}}});
    ASSERT_NE(manager, nullptr);

    CallbackListener lower{};
    CallbackListener upper{};
    lower.consume = false;
    upper.consume = true;

    manager->pushContext(InputContextId::Menu);
    manager->onAction("menu_cancel"_hs).connect<&CallbackListener::onAction>(&lower);

    manager->pushContext(InputContextId::Menu);
    manager->onAction("menu_cancel"_hs).connect<&CallbackListener::onAction>(&upper);

    pushKey(SDL_SCANCODE_ESCAPE, true);
    manager->sampleInputEvents();
    manager->dispatchActionCallbacks();

    EXPECT_EQ(upper.calls, 1);
    EXPECT_EQ(lower.calls, 0);

    manager->consumeTick();
    manager->onAction("menu_cancel"_hs).disconnect<&CallbackListener::onAction>(&upper);
    manager->popContext();

    pushKey(SDL_SCANCODE_ESCAPE, true);
    manager->sampleInputEvents();
    manager->dispatchActionCallbacks();

    EXPECT_EQ(lower.calls, 1);
}

} // namespace
} // namespace engine::input
// NOLINTEND
