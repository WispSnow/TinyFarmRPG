#include <gtest/gtest.h>

#include <SDL3/SDL.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

#include <entt/entity/registry.hpp>
#include <entt/core/hashed_string.hpp>
#include <entt/signal/dispatcher.hpp>

#include "engine/component/tags.h"
#include "engine/component/transform_component.h"
#include "engine/component/velocity_component.h"
#include "engine/core/game_state.h"
#include "engine/input/input_manager.h"
#include "engine/render/camera.h"
#include "engine/spatial/spatial_index_manager.h"

#include "game/component/actor_component.h"
#include "game/component/state_component.h"
#include "game/component/target_component.h"
#include "game/component/tags.h"
#include "game/defs/events.h"
#include "game/system/player_control_system.h"

namespace {

class PlayerControlSystemTest : public ::testing::Test {
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
        dispatcher_ = std::make_unique<entt::dispatcher>();
	        window_ = SDL_CreateWindow("PlayerControlSystemTest", 64, 64, SDL_WINDOW_HIDDEN);
	        ASSERT_NE(window_, nullptr);

	        game_state_ = engine::core::GameState::create(window_);
	        ASSERT_NE(game_state_, nullptr);

        const auto timestamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        config_path_ = std::filesystem::current_path() / ("player_control_system_test_config_" + std::to_string(timestamp) + ".json");

        std::ofstream config_file(config_path_);
        ASSERT_TRUE(config_file.is_open());
        config_file << R"({"input_mappings":{"player_light":["L"]}})";
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

constexpr glm::ivec2 MAP_SIZE(4, 4);
constexpr glm::ivec2 TILE_SIZE(16, 16);
constexpr glm::vec2 WORLD_MIN(0.0f, 0.0f);
constexpr glm::vec2 WORLD_MAX(64.0f, 64.0f);

void pushMouseMotion(int x, int y) {
    SDL_Event motion{};
    motion.type = SDL_EVENT_MOUSE_MOTION;
    motion.motion.x = static_cast<float>(x);
    motion.motion.y = static_cast<float>(y);
    ASSERT_EQ(SDL_PushEvent(&motion), true);
}

void pushMouseLeftDown(int x, int y) {
    SDL_Event button_down{};
    button_down.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    button_down.button.button = SDL_BUTTON_LEFT;
    button_down.button.down = true;
    button_down.button.x = static_cast<float>(x);
    button_down.button.y = static_cast<float>(y);
    ASSERT_EQ(SDL_PushEvent(&button_down), true);
}

} // namespace

namespace game::system {

namespace {
struct ToggleLightSpy {
    bool called{false};
    entt::id_type light_type_id{entt::null};
    void onToggle(const game::defs::ToggleLightRequest& event) {
        called = true;
        light_type_id = event.light_type_id;
    }
};
} // namespace

	TEST_F(PlayerControlSystemTest, ActionLockForcesVelocityZero) {
	    entt::registry registry;
		    engine::spatial::SpatialIndexManager spatial;
		    spatial.initialize(registry, MAP_SIZE, TILE_SIZE, WORLD_MIN, WORLD_MAX, glm::vec2(16.0f, 16.0f));
		    auto input = engine::input::InputManager::create(dispatcher_.get(), game_state_.get(), config_path_.string());
		    ASSERT_NE(input, nullptr);
		    engine::render::Camera camera(game_state_->getLogicalSize());
		    camera.setPosition(game_state_->getLogicalSize() * 0.5f);
		    camera.setZoom(1.0f);

    const entt::entity target = registry.create();
    registry.emplace<game::component::TargetComponent>(target);
    registry.emplace<engine::component::InvisibleTag>(target);

    const entt::entity player = registry.create();
    registry.emplace<game::component::PlayerTag>(player);
    registry.emplace<engine::component::TransformComponent>(player, glm::vec2(24.0f, 24.0f));
    auto& velocity = registry.emplace<engine::component::VelocityComponent>(player);
    velocity.velocity_ = glm::vec2(50.0f, 0.0f);
    auto& actor = registry.emplace<game::component::ActorComponent>(player);
    actor.speed_ = 100.0f;
    registry.emplace<game::component::StateComponent>(player);
    registry.emplace<game::component::ActionLockedTag>(player);

		    PlayerControlSystem system(registry, *dispatcher_, *input, camera, spatial, nullptr);
		    system.update(0.0f);

    const auto& vel = registry.get<engine::component::VelocityComponent>(player).velocity_;
    EXPECT_FLOAT_EQ(vel.x, 0.0f);
    EXPECT_FLOAT_EQ(vel.y, 0.0f);
}

	TEST_F(PlayerControlSystemTest, ToolSelected_MouseOutOfRange_KeepsTargetHidden) {
	    entt::registry registry;
		    engine::spatial::SpatialIndexManager spatial;
		    spatial.initialize(registry, MAP_SIZE, TILE_SIZE, WORLD_MIN, WORLD_MAX, glm::vec2(16.0f, 16.0f));
		    auto input = engine::input::InputManager::create(dispatcher_.get(), game_state_.get(), config_path_.string());
		    ASSERT_NE(input, nullptr);
		    engine::render::Camera camera(game_state_->getLogicalSize());
		    camera.setPosition(game_state_->getLogicalSize() * 0.5f);
		    camera.setZoom(1.0f);

    const entt::entity target = registry.create();
    registry.emplace<game::component::TargetComponent>(target);
    registry.emplace<engine::component::InvisibleTag>(target);

    const entt::entity player = registry.create();
    registry.emplace<game::component::PlayerTag>(player);
    registry.emplace<engine::component::TransformComponent>(player, glm::vec2(24.0f, 24.0f));
    registry.emplace<engine::component::VelocityComponent>(player);
    registry.emplace<game::component::ActorComponent>(player).speed_ = 100.0f;
    registry.emplace<game::component::StateComponent>(player);

		    PlayerControlSystem system(registry, *dispatcher_, *input, camera, spatial, nullptr);
		    system.update(0.0f);

    dispatcher_->trigger(game::defs::SwitchToolEvent{game::defs::Tool::Hoe});

	    // Player tile is (1,1); mouse tile (3,3) is out of range (delta 2).
	    pushMouseMotion(56, 56);
	    input->update();
	    system.update(0.0f);

    EXPECT_TRUE(registry.all_of<engine::component::InvisibleTag>(target));
}

	TEST_F(PlayerControlSystemTest, ToolSelected_MouseInRange_ShowsTargetAndSnapsToTile) {
	    entt::registry registry;
		    engine::spatial::SpatialIndexManager spatial;
		    spatial.initialize(registry, MAP_SIZE, TILE_SIZE, WORLD_MIN, WORLD_MAX, glm::vec2(16.0f, 16.0f));
		    auto input = engine::input::InputManager::create(dispatcher_.get(), game_state_.get(), config_path_.string());
		    ASSERT_NE(input, nullptr);
		    engine::render::Camera camera(game_state_->getLogicalSize());
		    camera.setPosition(game_state_->getLogicalSize() * 0.5f);
		    camera.setZoom(1.0f);

    const entt::entity target = registry.create();
    registry.emplace<game::component::TargetComponent>(target);
    registry.emplace<engine::component::InvisibleTag>(target);

    const entt::entity player = registry.create();
    registry.emplace<game::component::PlayerTag>(player);
    registry.emplace<engine::component::TransformComponent>(player, glm::vec2(24.0f, 24.0f));
    registry.emplace<engine::component::VelocityComponent>(player);
    registry.emplace<game::component::ActorComponent>(player).speed_ = 100.0f;
    registry.emplace<game::component::StateComponent>(player);

		    PlayerControlSystem system(registry, *dispatcher_, *input, camera, spatial, nullptr);
		    system.update(0.0f);

    dispatcher_->trigger(game::defs::SwitchToolEvent{game::defs::Tool::Hoe});

	    // Mouse tile (2,1) is within range (delta 1,0). Use tile center.
	    pushMouseMotion(40, 24);
	    input->update();
	    system.update(0.0f);

    EXPECT_FALSE(registry.all_of<engine::component::InvisibleTag>(target));

    const auto& cursor = registry.get<game::component::TargetComponent>(target);
    EXPECT_FLOAT_EQ(cursor.position_.x, 32.0f);
    EXPECT_FLOAT_EQ(cursor.position_.y, 16.0f);
}

	TEST_F(PlayerControlSystemTest, PlayerLightPressed_EmitsToggleLightRequest) {
	    entt::registry registry;
		    engine::spatial::SpatialIndexManager spatial;
		    spatial.initialize(registry, MAP_SIZE, TILE_SIZE, WORLD_MIN, WORLD_MAX, glm::vec2(16.0f, 16.0f));
		    auto input = engine::input::InputManager::create(dispatcher_.get(), game_state_.get(), config_path_.string());
		    ASSERT_NE(input, nullptr);
		    engine::render::Camera camera(game_state_->getLogicalSize());
		    camera.setPosition(game_state_->getLogicalSize() * 0.5f);
		    camera.setZoom(1.0f);

    ToggleLightSpy spy{};
    dispatcher_->sink<game::defs::ToggleLightRequest>().connect<&ToggleLightSpy::onToggle>(&spy);

    const entt::entity target = registry.create();
    registry.emplace<game::component::TargetComponent>(target);
    registry.emplace<engine::component::InvisibleTag>(target);

    const entt::entity player = registry.create();
    registry.emplace<game::component::PlayerTag>(player);
    registry.emplace<engine::component::TransformComponent>(player, glm::vec2(24.0f, 24.0f));
    registry.emplace<engine::component::VelocityComponent>(player);
    registry.emplace<game::component::ActorComponent>(player).speed_ = 100.0f;
    registry.emplace<game::component::StateComponent>(player);

		    PlayerControlSystem system(registry, *dispatcher_, *input, camera, spatial, nullptr);
		    system.update(0.0f);

    SDL_Event key_down{};
    key_down.type = SDL_EVENT_KEY_DOWN;
    key_down.key.scancode = SDL_SCANCODE_L;
    key_down.key.down = true;
    key_down.key.repeat = false;
    ASSERT_EQ(SDL_PushEvent(&key_down), true);

    input->update();
    EXPECT_TRUE(spy.called);
    EXPECT_EQ(spy.light_type_id, entt::hashed_string{"player_follow_light"}.value());

    dispatcher_->sink<game::defs::ToggleLightRequest>().disconnect<&ToggleLightSpy::onToggle>(&spy);
}

	TEST_F(PlayerControlSystemTest, ToolSelected_MouseOutOfRange_ClickDoesNotStartAction) {
	    entt::registry registry;
		    engine::spatial::SpatialIndexManager spatial;
		    spatial.initialize(registry, MAP_SIZE, TILE_SIZE, WORLD_MIN, WORLD_MAX, glm::vec2(16.0f, 16.0f));
		    auto input = engine::input::InputManager::create(dispatcher_.get(), game_state_.get(), config_path_.string());
		    ASSERT_NE(input, nullptr);
		    engine::render::Camera camera(game_state_->getLogicalSize());
		    camera.setPosition(game_state_->getLogicalSize() * 0.5f);
		    camera.setZoom(1.0f);

    const entt::entity target = registry.create();
    registry.emplace<game::component::TargetComponent>(target);
    registry.emplace<engine::component::InvisibleTag>(target);

    const entt::entity player = registry.create();
    registry.emplace<game::component::PlayerTag>(player);
    registry.emplace<engine::component::TransformComponent>(player, glm::vec2(24.0f, 24.0f));
    registry.emplace<engine::component::VelocityComponent>(player);
    registry.emplace<game::component::ActorComponent>(player).speed_ = 100.0f;
    auto& state = registry.emplace<game::component::StateComponent>(player);
    state.action_ = game::component::Action::Idle;

		    PlayerControlSystem system(registry, *dispatcher_, *input, camera, spatial, nullptr);
		    system.update(0.0f);

    dispatcher_->trigger(game::defs::SwitchToolEvent{game::defs::Tool::Hoe});

	    pushMouseMotion(56, 56);
	    input->update();
	    system.update(0.0f);

	    pushMouseLeftDown(56, 56);
	    input->update();

    EXPECT_FALSE(registry.all_of<game::component::ActionLockedTag>(player));
    EXPECT_EQ(registry.get<game::component::StateComponent>(player).action_, game::component::Action::Idle);
}

} // namespace game::system
