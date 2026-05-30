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
#include <string_view>
#include <vector>

#include "engine/core/game_state.h"
#include "engine/input/input_manager.h"
#include "game/scene/battle_input_router.h"
#include "game/scene/battle_scene_state.h"

namespace game::scene {
namespace {

[[nodiscard]] bool initSdlVideoWithDummyFallback(Uint32 flags) {
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
    return SDL_Init(flags);
}

class FakeBattleInputDelegate final : public BattleInputRouter::Delegate {
public:
    BattleMenuState state{BattleMenuState::PartyCommand};
    std::vector<int> move_deltas{};
    int confirm_count{0};
    int cancel_count{0};
    bool move_result{true};
    bool confirm_result{true};
    bool cancel_result{true};

    [[nodiscard]] BattleMenuState battleMenuState() const override {
        return state;
    }

    bool moveBattleMenuCursor(int delta) override {
        move_deltas.push_back(delta);
        return move_result;
    }

    bool confirmBattleMenu() override {
        ++confirm_count;
        return confirm_result;
    }

    bool cancelBattleMenu() override {
        ++cancel_count;
        return cancel_result;
    }
};

class BattleInputRouterTest : public ::testing::Test {
protected:
    SDL_Window* window_{nullptr};
    std::unique_ptr<entt::dispatcher> dispatcher_{};
    std::unique_ptr<engine::core::GameState> game_state_{};
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

        dispatcher_ = std::make_unique<entt::dispatcher>();
        window_ = SDL_CreateWindow("BattleInputRouterTest", 640, 480, SDL_WINDOW_HIDDEN);
        ASSERT_NE(window_, nullptr);
        game_state_ = engine::core::GameState::create(window_);
        ASSERT_NE(game_state_, nullptr);

        const auto timestamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        config_path_ = std::filesystem::temp_directory_path() /
                       ("battle_input_router_test_config_" + std::to_string(timestamp) + ".json");
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

    [[nodiscard]] std::unique_ptr<engine::input::InputManager> createManager() {
        nlohmann::json json;
        json["input_mappings"] = {
            {"menu_up", {"W"}},
            {"menu_down", {"S"}},
            {"menu_left", {"A"}},
            {"menu_right", {"D"}},
            {"menu_confirm", {"Return"}},
            {"menu_cancel", {"Escape"}},
        };

        std::ofstream config_file(config_path_);
        EXPECT_TRUE(config_file.is_open());
        config_file << json.dump(2);
        config_file.close();

        auto manager = engine::input::InputManager::create(dispatcher_.get(), game_state_.get(), config_path_.string());
        EXPECT_NE(manager, nullptr);
        if (manager) {
            manager->pushContext(engine::input::InputContextId::Battle);
        }
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

    void pressAndDispatch(engine::input::InputManager& manager, SDL_Scancode scancode) {
        pushKey(scancode, true);
        manager.sampleInputEvents();
        manager.dispatchActionCallbacks();
    }
};

TEST_F(BattleInputRouterTest, ActorCommandUsesLinearVerticalAndHorizontalStep) {
    auto manager = createManager();
    ASSERT_NE(manager, nullptr);
    FakeBattleInputDelegate delegate;
    delegate.state = BattleMenuState::ActorCommand;
    BattleInputRouter router;
    router.connect(*manager, delegate);

    pressAndDispatch(*manager, SDL_SCANCODE_S);
    manager->consumeTick();
    pressAndDispatch(*manager, SDL_SCANCODE_D);

    ASSERT_EQ(delegate.move_deltas.size(), 2U);
    EXPECT_EQ(delegate.move_deltas[0], 1);
    EXPECT_EQ(delegate.move_deltas[1], 1);
}

TEST_F(BattleInputRouterTest, HeldDirectionRepeatsAfterInitialDelay) {
    auto manager = createManager();
    ASSERT_NE(manager, nullptr);
    FakeBattleInputDelegate delegate;
    delegate.state = BattleMenuState::PartyCommand;
    BattleInputRouter router;
    router.connect(*manager, delegate);

    pressAndDispatch(*manager, SDL_SCANCODE_S);
    manager->consumeTick();
    ASSERT_EQ(delegate.move_deltas.size(), 1U);
    EXPECT_EQ(delegate.move_deltas[0], 1);

    router.update(0.27f);
    EXPECT_EQ(delegate.move_deltas.size(), 1U);

    router.update(0.02f);
    ASSERT_EQ(delegate.move_deltas.size(), 2U);
    EXPECT_EQ(delegate.move_deltas[1], 1);

    router.update(0.08f);
    ASSERT_EQ(delegate.move_deltas.size(), 3U);
    EXPECT_EQ(delegate.move_deltas[2], 1);
}

TEST_F(BattleInputRouterTest, ClearingRepeatPreventsCrossMenuHeldDirection) {
    auto manager = createManager();
    ASSERT_NE(manager, nullptr);
    FakeBattleInputDelegate delegate;
    delegate.state = BattleMenuState::PartyCommand;
    BattleInputRouter router;
    router.connect(*manager, delegate);

    pressAndDispatch(*manager, SDL_SCANCODE_S);
    manager->consumeTick();
    ASSERT_EQ(delegate.move_deltas.size(), 1U);

    delegate.state = BattleMenuState::ActorCommand;
    router.clearRepeat();
    router.update(1.0f);

    EXPECT_EQ(delegate.move_deltas.size(), 1U);
}

TEST_F(BattleInputRouterTest, ConfirmAndCancelForwardToDelegate) {
    auto manager = createManager();
    ASSERT_NE(manager, nullptr);
    FakeBattleInputDelegate delegate;
    BattleInputRouter router;
    router.connect(*manager, delegate);

    pressAndDispatch(*manager, SDL_SCANCODE_RETURN);
    manager->consumeTick();
    pressAndDispatch(*manager, SDL_SCANCODE_ESCAPE);

    EXPECT_EQ(delegate.confirm_count, 1);
    EXPECT_EQ(delegate.cancel_count, 1);
}

TEST_F(BattleInputRouterTest, BufferedConfirmReplaysWhenMenuBecomesReady) {
    auto manager = createManager();
    ASSERT_NE(manager, nullptr);
    FakeBattleInputDelegate delegate;
    delegate.state = BattleMenuState::None;
    delegate.confirm_result = false;
    BattleInputRouter router;
    router.connect(*manager, delegate);

    pressAndDispatch(*manager, SDL_SCANCODE_RETURN);
    EXPECT_EQ(delegate.confirm_count, 1);

    manager->consumeTick();
    delegate.state = BattleMenuState::ActorCommand;
    delegate.confirm_result = true;
    router.update(0.016f);

    EXPECT_EQ(delegate.confirm_count, 2);

    router.update(0.016f);
    EXPECT_EQ(delegate.confirm_count, 2);
}

TEST_F(BattleInputRouterTest, ImmediateConfirmDrainsBufferedPress) {
    auto manager = createManager();
    ASSERT_NE(manager, nullptr);
    FakeBattleInputDelegate delegate;
    delegate.state = BattleMenuState::ActorCommand;
    BattleInputRouter router;
    router.connect(*manager, delegate);

    pressAndDispatch(*manager, SDL_SCANCODE_RETURN);
    manager->consumeTick();
    router.update(0.016f);

    EXPECT_EQ(delegate.confirm_count, 1);
}

TEST_F(BattleInputRouterTest, BufferedCancelReplaysWhenMenuBecomesReady) {
    auto manager = createManager();
    ASSERT_NE(manager, nullptr);
    FakeBattleInputDelegate delegate;
    delegate.state = BattleMenuState::None;
    delegate.cancel_result = false;
    BattleInputRouter router;
    router.connect(*manager, delegate);

    pressAndDispatch(*manager, SDL_SCANCODE_ESCAPE);
    EXPECT_EQ(delegate.cancel_count, 1);

    manager->consumeTick();
    delegate.state = BattleMenuState::SkillList;
    delegate.cancel_result = true;
    router.update(0.016f);

    EXPECT_EQ(delegate.cancel_count, 2);

    router.update(0.016f);
    EXPECT_EQ(delegate.cancel_count, 2);
}

} // namespace
} // namespace game::scene
// NOLINTEND
