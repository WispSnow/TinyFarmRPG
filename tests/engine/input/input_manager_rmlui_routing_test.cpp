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

class InputManagerRmlUiRoutingTest : public ::testing::Test {
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

        drainEvents();
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

    void pushKey(SDL_Scancode scancode, bool down) {
        SDL_Event event{};
        event.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
        event.key.scancode = scancode;
        event.key.down = down;
        event.key.repeat = false;
        ASSERT_EQ(SDL_PushEvent(&event), true);
    }
};

TEST_F(InputManagerRmlUiRoutingTest, ConsumedPressDoesNotTriggerAction) {
    auto manager = createManager(R"({"input_mappings":{"move_left":["A"]}})");
    ASSERT_NE(manager, nullptr);

    const entt::id_type action_id = entt::hashed_string{"move_left"};
    manager->setRmlUiEventForwarder([](SDL_Event& event) {
        return event.type != SDL_EVENT_KEY_DOWN;
    });

    pushKey(SDL_SCANCODE_A, true);
    manager->sampleInputEvents();

    EXPECT_FALSE(manager->isActionPressed(action_id));
    EXPECT_FALSE(manager->isActionDown(action_id));
}

TEST_F(InputManagerRmlUiRoutingTest, ConsumedReleaseStillClearsHeldAction) {
    auto manager = createManager(R"({"input_mappings":{"move_left":["A"]}})");
    ASSERT_NE(manager, nullptr);

    const entt::id_type action_id = entt::hashed_string{"move_left"};
    manager->setRmlUiEventForwarder([](SDL_Event&) { return true; });

    pushKey(SDL_SCANCODE_A, true);
    manager->sampleInputEvents();
    EXPECT_TRUE(manager->isActionPressed(action_id));

    manager->consumeTick();
    EXPECT_TRUE(manager->isActionDown(action_id));

    manager->setRmlUiEventForwarder([](SDL_Event&) { return false; });

    pushKey(SDL_SCANCODE_A, false);
    manager->sampleInputEvents();

    EXPECT_TRUE(manager->isActionReleased(action_id));
    EXPECT_FALSE(manager->isActionDown(action_id));
}

TEST_F(InputManagerRmlUiRoutingTest, MenuContextSuppressesConfiguredNavigationKeysBeforeRmlUiForward) {
    auto manager = createManager(R"({
        "input_mappings":{
            "move_left":["A"],
            "menu_up":["A"],
            "menu_confirm":["Return"],
            "menu_cancel":["Escape"]
        }
    })");
    ASSERT_NE(manager, nullptr);

    int rmlui_forward_count = 0;
    manager->setRmlUiEventForwarder([&](SDL_Event&) {
        ++rmlui_forward_count;
        return true;
    });

    manager->pushContext(InputContextId::Menu);
    pushKey(SDL_SCANCODE_A, true);
    manager->sampleInputEvents();

    EXPECT_EQ(rmlui_forward_count, 0);
    EXPECT_FALSE(manager->isActionPressed("move_left"_hs));
    EXPECT_TRUE(manager->isActionPressed("menu_up"_hs));
}

TEST_F(InputManagerRmlUiRoutingTest, GameplayContextKeepsLegacyRawRmlUiForwardingForSharedScancode) {
    auto manager = createManager(R"({
        "input_mappings":{
            "move_left":["A"],
            "menu_up":["A"]
        }
    })");
    ASSERT_NE(manager, nullptr);

    int rmlui_forward_count = 0;
    manager->setRmlUiEventForwarder([&](SDL_Event&) {
        ++rmlui_forward_count;
        return true;
    });

    manager->pushContext(InputContextId::Gameplay);
    pushKey(SDL_SCANCODE_A, true);
    manager->sampleInputEvents();

    EXPECT_EQ(rmlui_forward_count, 1);
    EXPECT_TRUE(manager->isActionPressed("move_left"_hs));
    EXPECT_FALSE(manager->isActionPressed("menu_up"_hs));
}

TEST_F(InputManagerRmlUiRoutingTest, MenuSuppressSetFollowsConfiguredBindings) {
    auto manager = createManager(R"({
        "input_mappings":{
            "move_up":["A"],
            "menu_up":["W"]
        }
    })");
    ASSERT_NE(manager, nullptr);

    int rmlui_forward_count = 0;
    manager->setRmlUiEventForwarder([&](SDL_Event&) {
        ++rmlui_forward_count;
        return true;
    });

    manager->pushContext(InputContextId::Menu);
    pushKey(SDL_SCANCODE_A, true);
    manager->sampleInputEvents();

    EXPECT_EQ(rmlui_forward_count, 1);
    EXPECT_FALSE(manager->isActionPressed("move_up"_hs));
    EXPECT_FALSE(manager->isActionPressed("menu_up"_hs));

    pushKey(SDL_SCANCODE_W, true);
    manager->sampleInputEvents();

    EXPECT_EQ(rmlui_forward_count, 1);
    EXPECT_TRUE(manager->isActionPressed("menu_up"_hs));
}

TEST_F(InputManagerRmlUiRoutingTest, TabKeepsRawRmlUiForwardingInMenuContext) {
    auto manager = createManager(R"({
        "input_mappings":{
            "hotbar":["Tab"],
            "menu_up":["W"]
        }
    })");
    ASSERT_NE(manager, nullptr);

    int rmlui_forward_count = 0;
    manager->setRmlUiEventForwarder([&](SDL_Event&) {
        ++rmlui_forward_count;
        return true;
    });

    manager->pushContext(InputContextId::Menu);
    pushKey(SDL_SCANCODE_TAB, true);
    manager->sampleInputEvents();

    EXPECT_EQ(rmlui_forward_count, 1);
    EXPECT_FALSE(manager->isActionPressed("hotbar"_hs));
}

TEST_F(InputManagerRmlUiRoutingTest, SdlEventObserverRunsBeforeRmlUiAndAlwaysReceivesEvent) {
    auto manager = createManager(R"({"input_mappings":{"move_left":["A"]}})");
    ASSERT_NE(manager, nullptr);

    std::vector<std::string> call_order;
    manager->setSdlEventObserver([&](const SDL_Event&) {
        call_order.emplace_back("observer");
    });
    manager->setRmlUiEventForwarder([&](SDL_Event&) {
        call_order.emplace_back("rmlui");
        return false;
    });

    pushKey(SDL_SCANCODE_A, true);
    manager->sampleInputEvents();

    ASSERT_EQ(call_order.size(), 2u);
    EXPECT_EQ(call_order[0], "observer");
    EXPECT_EQ(call_order[1], "rmlui");
}

} // namespace
} // namespace engine::input
// NOLINTEND
