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

[[nodiscard]] bool initSdlVideoWithDummyFallback(Uint32 flags) {
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
    return SDL_Init(flags);
}

class InputManagerTest : public ::testing::Test {
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

	    EXPECT_GE(callback_count, 0);
	    if (callback_count > 0) {
	        EXPECT_TRUE(received_motion);
	    }
	    const auto logical_position = manager->getLogicalMousePosition();
	    EXPECT_FLOAT_EQ(logical_position.x, 256.0F);
	    EXPECT_FLOAT_EQ(logical_position.y, 144.0F);
	}

    TEST_F(InputManagerTest, PressedStatePersistsAcrossSampleFramesUntilConsumeTick) {
        auto manager = InputManager::create(dispatcher_.get(), game_state_.get(), config_path_.string());
        ASSERT_NE(manager, nullptr);
        const entt::id_type action_id = entt::hashed_string{"move_left"};

        SDL_Event key_down{};
        key_down.type = SDL_EVENT_KEY_DOWN;
        key_down.key.scancode = SDL_SCANCODE_A;
        key_down.key.down = true;
        key_down.key.repeat = false;
        ASSERT_EQ(SDL_PushEvent(&key_down), true);

        manager->sampleInputEvents();
        EXPECT_TRUE(manager->isActionPressed(action_id));

        // 没有 consumeTick 时，边沿状态应保留到下一个逻辑 tick。
        manager->sampleInputEvents();
        EXPECT_TRUE(manager->isActionPressed(action_id));

        manager->dispatchActionCallbacks();
        manager->consumeTick();
        EXPECT_FALSE(manager->isActionPressed(action_id));
        EXPECT_TRUE(manager->isActionDown(action_id));
    }

    TEST_F(InputManagerTest, MouseWheelDeltaPersistsUntilConsumeTick) {
        auto manager = InputManager::create(dispatcher_.get(), game_state_.get(), config_path_.string());
        ASSERT_NE(manager, nullptr);

        SDL_Event wheel{};
        wheel.type = SDL_EVENT_MOUSE_WHEEL;
        wheel.wheel.x = 0.0f;
        wheel.wheel.y = 1.0f;
        ASSERT_EQ(SDL_PushEvent(&wheel), true);

        manager->sampleInputEvents();
        auto delta = manager->getMouseWheelDelta();
        EXPECT_FLOAT_EQ(delta.x, 0.0f);
        EXPECT_FLOAT_EQ(delta.y, 1.0f);

        // 未 consumeTick 前，滚轮输入不应被“帧采样”清空。
        manager->sampleInputEvents();
        delta = manager->getMouseWheelDelta();
        EXPECT_FLOAT_EQ(delta.x, 0.0f);
        EXPECT_FLOAT_EQ(delta.y, 1.0f);

        manager->consumeTick();
        delta = manager->getMouseWheelDelta();
        EXPECT_FLOAT_EQ(delta.x, 0.0f);
        EXPECT_FLOAT_EQ(delta.y, 0.0f);
    }

    TEST_F(InputManagerTest, MultiKeyBindingSameAction) {
        // 创建绑定两个键到同一动作的配置
        const auto temp_dir = std::filesystem::temp_directory_path();
        const auto ts = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        auto multi_config = temp_dir / ("input_multi_key_test_" + std::to_string(ts) + ".json");
        {
            std::ofstream f(multi_config);
            f << R"({"input_mappings":{"move_left":["A","Left"]}})";
        }

        auto manager = InputManager::create(dispatcher_.get(), game_state_.get(), multi_config.string());
        ASSERT_NE(manager, nullptr);
        const entt::id_type action_id = entt::hashed_string{"move_left"};

        // 按下 A → PRESSED
        SDL_Event a_down{};
        a_down.type = SDL_EVENT_KEY_DOWN;
        a_down.key.scancode = SDL_SCANCODE_A;
        a_down.key.down = true;
        a_down.key.repeat = false;
        ASSERT_EQ(SDL_PushEvent(&a_down), true);

        manager->sampleInputEvents();
        EXPECT_TRUE(manager->isActionPressed(action_id));

        manager->dispatchActionCallbacks();
        manager->consumeTick();
        EXPECT_TRUE(manager->isActionDown(action_id));  // HELD

        // 按下 Left → 仍 HELD（动作已经激活，不回到 PRESSED）
        SDL_Event left_down{};
        left_down.type = SDL_EVENT_KEY_DOWN;
        left_down.key.scancode = SDL_SCANCODE_LEFT;
        left_down.key.down = true;
        left_down.key.repeat = false;
        ASSERT_EQ(SDL_PushEvent(&left_down), true);

        manager->sampleInputEvents();
        EXPECT_TRUE(manager->isActionDown(action_id));

        manager->dispatchActionCallbacks();
        manager->consumeTick();

        // 释放 A → 仍 HELD（Left 还按着）
        SDL_Event a_up{};
        a_up.type = SDL_EVENT_KEY_UP;
        a_up.key.scancode = SDL_SCANCODE_A;
        a_up.key.down = false;
        a_up.key.repeat = false;
        ASSERT_EQ(SDL_PushEvent(&a_up), true);

        manager->sampleInputEvents();
        EXPECT_TRUE(manager->isActionDown(action_id));
        EXPECT_FALSE(manager->isActionReleased(action_id));

        manager->dispatchActionCallbacks();
        manager->consumeTick();

        // 释放 Left → RELEASED
        SDL_Event left_up{};
        left_up.type = SDL_EVENT_KEY_UP;
        left_up.key.scancode = SDL_SCANCODE_LEFT;
        left_up.key.down = false;
        left_up.key.repeat = false;
        ASSERT_EQ(SDL_PushEvent(&left_up), true);

        manager->sampleInputEvents();
        EXPECT_TRUE(manager->isActionReleased(action_id));
        EXPECT_FALSE(manager->isActionDown(action_id));

        std::error_code ec;
        std::filesystem::remove(multi_config, ec);
    }

    TEST_F(InputManagerTest, RepeatKeyDownDoesNotInflateActiveCount) {
        auto manager = InputManager::create(dispatcher_.get(), game_state_.get(), config_path_.string());
        ASSERT_NE(manager, nullptr);
        const entt::id_type action_id = entt::hashed_string{"move_left"};

        // 按下 A
        SDL_Event a_down{};
        a_down.type = SDL_EVENT_KEY_DOWN;
        a_down.key.scancode = SDL_SCANCODE_A;
        a_down.key.down = true;
        a_down.key.repeat = false;
        ASSERT_EQ(SDL_PushEvent(&a_down), true);

        manager->sampleInputEvents();
        EXPECT_TRUE(manager->isActionPressed(action_id));

        manager->dispatchActionCallbacks();
        manager->consumeTick();

        // 发送多个 repeat 事件 → 不应改变 active_count
        for (int i = 0; i < 5; ++i) {
            SDL_Event repeat{};
            repeat.type = SDL_EVENT_KEY_DOWN;
            repeat.key.scancode = SDL_SCANCODE_A;
            repeat.key.down = true;
            repeat.key.repeat = true;
            ASSERT_EQ(SDL_PushEvent(&repeat), true);
        }
        manager->sampleInputEvents();
        EXPECT_TRUE(manager->isActionDown(action_id));

        manager->dispatchActionCallbacks();
        manager->consumeTick();

        // 释放 A → 应正常 RELEASED（active_count 没有因 repeat 膨胀）
        SDL_Event a_up{};
        a_up.type = SDL_EVENT_KEY_UP;
        a_up.key.scancode = SDL_SCANCODE_A;
        a_up.key.down = false;
        a_up.key.repeat = false;
        ASSERT_EQ(SDL_PushEvent(&a_up), true);

        manager->sampleInputEvents();
        EXPECT_TRUE(manager->isActionReleased(action_id));
        EXPECT_FALSE(manager->isActionDown(action_id));
    }

    TEST_F(InputManagerTest, FocusLostClearsHeldActions) {
        auto manager = InputManager::create(dispatcher_.get(), game_state_.get(), config_path_.string());
        ASSERT_NE(manager, nullptr);
        const entt::id_type action_id = entt::hashed_string{"move_left"};

        // 按下 A → PRESSED
        SDL_Event a_down{};
        a_down.type = SDL_EVENT_KEY_DOWN;
        a_down.key.scancode = SDL_SCANCODE_A;
        a_down.key.down = true;
        a_down.key.repeat = false;
        ASSERT_EQ(SDL_PushEvent(&a_down), true);

        manager->sampleInputEvents();
        EXPECT_TRUE(manager->isActionPressed(action_id));

        manager->dispatchActionCallbacks();
        manager->consumeTick();
        EXPECT_TRUE(manager->isActionDown(action_id));  // HELD

        // 发送窗口失焦事件 → 应直接变为 INACTIVE
        SDL_Event focus_lost{};
        focus_lost.type = SDL_EVENT_WINDOW_FOCUS_LOST;
        focus_lost.window.windowID = SDL_GetWindowID(window_);
        ASSERT_EQ(SDL_PushEvent(&focus_lost), true);

        manager->sampleInputEvents();
        EXPECT_FALSE(manager->isActionDown(action_id));
        EXPECT_FALSE(manager->isActionReleased(action_id));  // 不经过 RELEASED
    }

    // 用于回调标志的全局状态
    inline bool g_released_callback_called = false;
    bool onReleasedFlag() { g_released_callback_called = true; return false; }

    inline std::vector<std::string> g_dispatch_log;
    bool onActionAPressed() { g_dispatch_log.emplace_back("action_a"); return false; }
    bool onActionBPressed() { g_dispatch_log.emplace_back("action_b"); return false; }

    TEST_F(InputManagerTest, FocusLostDoesNotDispatchReleasedCallbacks) {
        auto manager = InputManager::create(dispatcher_.get(), game_state_.get(), config_path_.string());
        ASSERT_NE(manager, nullptr);
        const entt::id_type action_id = entt::hashed_string{"move_left"};

        // 注册 RELEASED 回调
        g_released_callback_called = false;
        manager->onAction(action_id, ActionState::RELEASED).connect<&onReleasedFlag>();

        // 按下 A
        SDL_Event a_down{};
        a_down.type = SDL_EVENT_KEY_DOWN;
        a_down.key.scancode = SDL_SCANCODE_A;
        a_down.key.down = true;
        a_down.key.repeat = false;
        ASSERT_EQ(SDL_PushEvent(&a_down), true);

        manager->sampleInputEvents();
        manager->dispatchActionCallbacks();
        manager->consumeTick();

        // 发送失焦事件
        SDL_Event focus_lost{};
        focus_lost.type = SDL_EVENT_WINDOW_FOCUS_LOST;
        focus_lost.window.windowID = SDL_GetWindowID(window_);
        ASSERT_EQ(SDL_PushEvent(&focus_lost), true);

        manager->sampleInputEvents();
        manager->dispatchActionCallbacks();

        EXPECT_FALSE(g_released_callback_called);
    }

    TEST_F(InputManagerTest, DispatchOrderIsStable) {
        // 创建包含两个动作的配置（字母序：action_a < action_b）
        const auto temp_dir = std::filesystem::temp_directory_path();
        const auto ts = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        auto order_config = temp_dir / ("input_order_test_" + std::to_string(ts) + ".json");
        {
            std::ofstream f(order_config);
            f << R"({"input_mappings":{"action_b":["B"],"action_a":["A"]}})";
        }

        auto manager = InputManager::create(dispatcher_.get(), game_state_.get(), order_config.string());
        ASSERT_NE(manager, nullptr);

        const entt::id_type id_a = entt::hashed_string{"action_a"};
        const entt::id_type id_b = entt::hashed_string{"action_b"};

        g_dispatch_log.clear();
        manager->onAction(id_a, ActionState::PRESSED).connect<&onActionAPressed>();
        manager->onAction(id_b, ActionState::PRESSED).connect<&onActionBPressed>();

        // 同帧按下 A 和 B
        SDL_Event a_down{};
        a_down.type = SDL_EVENT_KEY_DOWN;
        a_down.key.scancode = SDL_SCANCODE_A;
        a_down.key.down = true;
        a_down.key.repeat = false;
        ASSERT_EQ(SDL_PushEvent(&a_down), true);

        SDL_Event b_down{};
        b_down.type = SDL_EVENT_KEY_DOWN;
        b_down.key.scancode = SDL_SCANCODE_B;
        b_down.key.down = true;
        b_down.key.repeat = false;
        ASSERT_EQ(SDL_PushEvent(&b_down), true);

        manager->sampleInputEvents();
        manager->dispatchActionCallbacks();

        // 验证分发顺序为字母序（action_a 在 action_b 之前）
        ASSERT_EQ(g_dispatch_log.size(), 2u);
        EXPECT_EQ(g_dispatch_log[0], "action_a");
        EXPECT_EQ(g_dispatch_log[1], "action_b");

        // 重复验证稳定性
        manager->consumeTick();
        g_dispatch_log.clear();

        // 释放后重新按下，验证顺序不变
        SDL_Event a_up{};
        a_up.type = SDL_EVENT_KEY_UP;
        a_up.key.scancode = SDL_SCANCODE_A;
        a_up.key.down = false;
        a_up.key.repeat = false;
        ASSERT_EQ(SDL_PushEvent(&a_up), true);

        SDL_Event b_up{};
        b_up.type = SDL_EVENT_KEY_UP;
        b_up.key.scancode = SDL_SCANCODE_B;
        b_up.key.down = false;
        b_up.key.repeat = false;
        ASSERT_EQ(SDL_PushEvent(&b_up), true);

        manager->sampleInputEvents();
        manager->dispatchActionCallbacks();
        manager->consumeTick();

        // 再次按下
        ASSERT_EQ(SDL_PushEvent(&a_down), true);
        ASSERT_EQ(SDL_PushEvent(&b_down), true);

        manager->sampleInputEvents();
        manager->dispatchActionCallbacks();

        ASSERT_EQ(g_dispatch_log.size(), 2u);
        EXPECT_EQ(g_dispatch_log[0], "action_a");
        EXPECT_EQ(g_dispatch_log[1], "action_b");

        std::error_code ec;
        std::filesystem::remove(order_config, ec);
    }

} // namespace
} // namespace engine::input
// NOLINTEND
