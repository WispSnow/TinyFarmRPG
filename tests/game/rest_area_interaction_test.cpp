#include <gtest/gtest.h>

#include <SDL3/SDL.h>
#include <entt/core/hashed_string.hpp>
#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>
#include <chrono>

#include "engine/core/game_state.h"
#include "engine/component/transform_component.h"
#include "engine/input/input_manager.h"
#include "engine/spatial/spatial_index_manager.h"

#include "game/component/map_component.h"
#include "game/component/state_component.h"
#include "game/component/tags.h"
#include "game/defs/events.h"
#include "game/defs/spatial_layers.h"
#include "game/system/interaction_system.h"
#include "game/world/world_state.h"

using namespace entt::literals;

namespace {

class RestAreaInteractionTest : public ::testing::Test {
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

	        window_ = SDL_CreateWindow("RestAreaInteractionTest", 640, 480, SDL_WINDOW_HIDDEN);
	        ASSERT_NE(window_, nullptr);

	        game_state_ = engine::core::GameState::create(window_);
	        ASSERT_NE(game_state_, nullptr);

        const auto timestamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        config_path_ = std::filesystem::temp_directory_path() / ("rest_area_interaction_test_config_" + std::to_string(timestamp) + ".json");
        std::ofstream config_file(config_path_);
        ASSERT_TRUE(config_file.is_open());
        config_file << R"({"input_mappings":{"interact":["F"]}})";
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

struct InteractCapture {
    std::vector<game::defs::InteractRequest> events{};
    void onEvent(const game::defs::InteractRequest& evt) { events.push_back(evt); }
};

} // namespace

namespace game::system {

TEST_F(RestAreaInteractionTest, FacingTileRestArea_TriggersInteractRequest) {
    entt::registry registry;
    engine::spatial::SpatialIndexManager spatial;
    constexpr glm::ivec2 MAP_SIZE(4, 4);
    constexpr glm::ivec2 TILE_SIZE(16, 16);
    spatial.initialize(registry,
                       MAP_SIZE,
                       TILE_SIZE,
                       /*world_bounds_min*/ glm::vec2(0.0f, 0.0f),
                       /*world_bounds_max*/ glm::vec2(64.0f, 64.0f),
                       /*dynamic_cell_size*/ glm::vec2(16.0f, 16.0f));

    game::world::WorldState world_state;
    const entt::id_type map_id = world_state.ensureExternalMap("rest_area_test_map");
    world_state.setCurrentMap(map_id);
	    if (auto* map_state = world_state.getMapStateMutable(map_id)) {
	        map_state->info.size_px = glm::ivec2(64, 64);
	    }

	    auto input = engine::input::InputManager::create(dispatcher_.get(), game_state_.get(), config_path_.string());
	    ASSERT_NE(input, nullptr);

    const entt::entity player = registry.create();
    registry.emplace<game::component::PlayerTag>(player);
    registry.emplace<engine::component::TransformComponent>(player, glm::vec2(16.0f, 16.0f));
    auto& state = registry.emplace<game::component::StateComponent>(player);
    state.direction_ = game::component::Direction::Right;
    registry.emplace<game::component::MapId>(player, map_id);

    const glm::ivec2 rest_tile(2, 1);
    const glm::vec2 rest_tile_pos(rest_tile.x * TILE_SIZE.x, rest_tile.y * TILE_SIZE.y);

    const entt::entity rest_area = registry.create();
    registry.emplace<game::component::MapId>(rest_area, map_id);
    registry.emplace<game::component::RestArea>(rest_area, engine::utils::Rect{rest_tile_pos, glm::vec2(TILE_SIZE)});
    spatial.addTileEntity(rest_tile, rest_area, game::defs::spatial_layer::REST);

	    InteractCapture capture{};
	    dispatcher_->sink<game::defs::InteractRequest>().connect<&InteractCapture::onEvent>(&capture);

	    InteractionSystem system(registry, *dispatcher_, *input, spatial, world_state);

    SDL_Event key_down{};
    key_down.type = SDL_EVENT_KEY_DOWN;
    key_down.key.scancode = SDL_SCANCODE_F;
    key_down.key.down = true;
    key_down.key.repeat = false;
    ASSERT_EQ(SDL_PushEvent(&key_down), true);

	    input->update();
	    system.update();

    ASSERT_EQ(capture.events.size(), 1u);
    EXPECT_EQ(capture.events[0].player, player);
    EXPECT_EQ(capture.events[0].target, rest_area);
}

} // namespace game::system
