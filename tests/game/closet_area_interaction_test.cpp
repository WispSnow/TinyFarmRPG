#include <gtest/gtest.h>

#include <SDL3/SDL.h>
#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "engine/component/transform_component.h"
#include "engine/core/game_state.h"
#include "engine/input/input_manager.h"
#include "engine/spatial/spatial_index_manager.h"

#include "game/component/map_component.h"
#include "game/component/state_component.h"
#include "game/component/tags.h"
#include "game/defs/commands.h"
#include "game/defs/spatial_layers.h"
#include "game/system/interaction_system.h"
#include "game/world/world_state.h"

namespace {

[[nodiscard]] bool initSdlVideoWithDummyFallback(Uint32 flags) {
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
    return SDL_Init(flags);
}

class ClosetAreaInteractionTest : public ::testing::Test {
protected:
    static inline bool sdl_ready_{false};

    SDL_Window* window_{nullptr};
    std::unique_ptr<engine::core::GameState> game_state_{};
    entt::dispatcher dispatcher_{};
    std::filesystem::path config_path_{};

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

        window_ = SDL_CreateWindow("ClosetAreaInteractionTest", 640, 480, SDL_WINDOW_HIDDEN);
        ASSERT_NE(window_, nullptr);

        game_state_ = engine::core::GameState::create(window_);
        ASSERT_NE(game_state_, nullptr);

        const auto timestamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        config_path_ = std::filesystem::temp_directory_path() /
                       ("closet_area_interaction_test_config_" + std::to_string(timestamp) + ".json");
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
    std::vector<game::defs::InteractCommand> events{};
    void onEvent(const game::defs::InteractCommand& event) { events.push_back(event); }
};

} // namespace

namespace game::system {

TEST_F(ClosetAreaInteractionTest, FacingTileClosetAreaTriggersInteractCommandBeforeRestArea) {
    entt::registry registry;
    engine::spatial::SpatialIndexManager spatial;
    constexpr glm::ivec2 MAP_SIZE{4, 4};
    constexpr glm::ivec2 TILE_SIZE{16, 16};
    spatial.initialize(registry,
                       MAP_SIZE,
                       TILE_SIZE,
                       /*world_bounds_min*/ glm::vec2{0.0f, 0.0f},
                       /*world_bounds_max*/ glm::vec2{64.0f, 64.0f},
                       /*dynamic_cell_size*/ glm::vec2{16.0f, 16.0f});

    game::world::WorldState world_state;
    const entt::id_type map_id = world_state.ensureExternalMap("closet_area_test_map");
    world_state.setCurrentMap(map_id);
    if (auto* map_state = world_state.getMapStateMutable(map_id)) {
        map_state->info.size_px = glm::ivec2{64, 64};
    }

    auto input = engine::input::InputManager::create(&dispatcher_, game_state_.get(), config_path_.string());
    ASSERT_NE(input, nullptr);

    const entt::entity player = registry.create();
    registry.emplace<game::component::PlayerTag>(player);
    registry.emplace<engine::component::TransformComponent>(player, glm::vec2{16.0f, 16.0f});
    auto& state = registry.emplace<game::component::StateComponent>(player);
    state.direction_ = game::component::Direction::Right;
    registry.emplace<game::component::MapId>(player, map_id);

    const glm::ivec2 target_tile{2, 1};
    const glm::vec2 target_pos{
        static_cast<float>(target_tile.x * TILE_SIZE.x),
        static_cast<float>(target_tile.y * TILE_SIZE.y)};
    const glm::vec2 target_size{static_cast<float>(TILE_SIZE.x), static_cast<float>(TILE_SIZE.y)};

    const entt::entity rest_area = registry.create();
    registry.emplace<game::component::MapId>(rest_area, map_id);
    registry.emplace<game::component::RestArea>(rest_area, engine::utils::Rect{target_pos, target_size});
    spatial.addTileEntity(target_tile, rest_area, game::defs::spatial_layer::REST);

    const entt::entity closet_area = registry.create();
    registry.emplace<game::component::MapId>(closet_area, map_id);
    registry.emplace<game::component::ClosetArea>(closet_area, engine::utils::Rect{target_pos, target_size});
    spatial.addTileEntity(target_tile, closet_area, game::defs::spatial_layer::CLOSET);

    InteractCapture capture{};
    dispatcher_.sink<game::defs::InteractCommand>().connect<&InteractCapture::onEvent>(&capture);

    InteractionSystem system(registry, dispatcher_, *input, spatial, world_state);

    SDL_Event key_down{};
    key_down.type = SDL_EVENT_KEY_DOWN;
    key_down.key.scancode = SDL_SCANCODE_F;
    key_down.key.down = true;
    key_down.key.repeat = false;
    ASSERT_EQ(SDL_PushEvent(&key_down), true);

    input->update();
    system.update();

    ASSERT_EQ(capture.events.size(), 1U);
    EXPECT_EQ(capture.events[0].player, player);
    EXPECT_EQ(capture.events[0].target, closet_area);
}

} // namespace game::system
