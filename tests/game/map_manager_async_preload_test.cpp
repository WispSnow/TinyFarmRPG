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
#include <thread>

#include "engine/audio/audio_player.h"
#include "engine/async/main_thread_command_queue.h"
#include "engine/core/context.h"
#include "engine/core/game_state.h"
#include "engine/core/time.h"
#ifdef TF_ENABLE_DEBUG_UI
#include "engine/debug/debug_ui_manager.h"
#endif
#include "engine/input/input_manager.h"
#include "engine/render/camera.h"
#include "engine/render/opengl/gl_renderer.h"
#include "engine/render/renderer.h"
#include "engine/render/text_renderer.h"
#include "engine/resource/auto_tile_library.h"
#include "engine/resource/resource_manager.h"
#include "engine/scene/scene.h"
#include "engine/spatial/spatial_index_manager.h"
#include "engine/ui/ui_preset_manager.h"
#include "game/factory/blueprint_manager.h"
#include "game/factory/entity_factory.h"
#include "game/world/map_loading_settings.h"
#include "game/world/map_manager.h"
#include "game/world/world_state.h"

namespace game::world {
namespace {

[[nodiscard]] bool initSdlVideoWithDummyFallback(Uint32 flags) {
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
    return SDL_Init(flags);
}

class TestScene final : public engine::scene::Scene {
public:
    explicit TestScene(engine::core::Context& context)
        : Scene("MapManagerAsyncPreloadTestScene", context) {}
};

class MapManagerAsyncPreloadTest : public ::testing::Test {
protected:
    static inline bool sdl_ready_{false};

    SDL_Window* window_{nullptr};
    std::filesystem::path input_config_path_{};
    std::filesystem::path original_working_dir_{};
    std::filesystem::path runtime_root_{};

    entt::dispatcher dispatcher_{};
    std::unique_ptr<engine::core::GameState> game_state_{};
    std::unique_ptr<engine::input::InputManager> input_manager_{};
    std::unique_ptr<engine::resource::ResourceManager> resource_manager_{};
    engine::resource::AutoTileLibrary auto_tile_library_{};
    std::unique_ptr<engine::ui::UIPresetManager> ui_preset_manager_{};
    std::unique_ptr<engine::audio::AudioPlayer> audio_player_{};
    std::unique_ptr<engine::render::opengl::GLRenderer> gl_renderer_{};
    std::unique_ptr<engine::render::Renderer> renderer_{};
    std::unique_ptr<engine::render::Camera> camera_{};
    std::unique_ptr<engine::render::TextRenderer> text_renderer_{};
    engine::spatial::SpatialIndexManager spatial_index_manager_{};
    std::unique_ptr<engine::core::Time> time_{};
    std::unique_ptr<engine::async::MainThreadCommandQueue> main_thread_command_queue_{};
#ifdef TF_ENABLE_DEBUG_UI
    std::unique_ptr<engine::debug::DebugUIManager> debug_ui_manager_{};
#endif
    std::unique_ptr<engine::core::Context> context_{};

    std::unique_ptr<TestScene> scene_{};
    game::world::WorldState world_state_{};
    game::factory::BlueprintManager blueprint_manager_{};
    std::unique_ptr<game::factory::EntityFactory> entity_factory_{};
    std::unique_ptr<game::world::MapManager> map_manager_{};

    static void SetUpTestSuite() {
        sdl_ready_ = initSdlVideoWithDummyFallback(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
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

        original_working_dir_ = std::filesystem::current_path();
        runtime_root_ = original_working_dir_;
        if (!std::filesystem::exists(runtime_root_ / "assets") &&
            std::filesystem::exists(runtime_root_.parent_path() / "assets")) {
            runtime_root_ = runtime_root_.parent_path();
        }
        if (!std::filesystem::exists(runtime_root_ / "assets")) {
            GTEST_SKIP() << "Unable to locate runtime assets directory for map preload test.";
        }

        std::error_code ec;
        std::filesystem::current_path(runtime_root_, ec);
        if (ec) {
            GTEST_SKIP() << "Failed to switch working directory to runtime root.";
        }

        window_ = SDL_CreateWindow("MapManagerAsyncPreloadTest", 640, 360, SDL_WINDOW_HIDDEN);
        if (!window_) {
            GTEST_SKIP() << "Failed to create SDL window.";
        }

        game_state_ = engine::core::GameState::create(window_);
        if (!game_state_) {
            GTEST_SKIP() << "Failed to create GameState.";
        }
        game_state_->setWindowSize({640.0F, 360.0F});
        game_state_->setLogicalSize({640.0F, 360.0F});

        input_config_path_ = std::filesystem::temp_directory_path() / "map_manager_async_preload_input.json";
        std::ofstream input_config(input_config_path_);
        ASSERT_TRUE(input_config.is_open());
        input_config << R"({"input_mappings":{"mouse_left":["MouseLeft"]}})";
        input_config.close();

        input_manager_ = engine::input::InputManager::create(&dispatcher_, game_state_.get(), input_config_path_.string());
        if (!input_manager_) {
            GTEST_SKIP() << "Failed to create InputManager.";
        }

        resource_manager_ = engine::resource::ResourceManager::create(&dispatcher_);
        if (!resource_manager_) {
            GTEST_SKIP() << "Failed to create ResourceManager.";
        }

        ui_preset_manager_ = std::make_unique<engine::ui::UIPresetManager>();
        audio_player_ = engine::audio::AudioPlayer::create(resource_manager_.get());
        if (!audio_player_) {
            GTEST_SKIP() << "Failed to create AudioPlayer.";
        }

        gl_renderer_ = engine::render::opengl::GLRenderer::createHeadless(game_state_->getLogicalSize());
        if (!gl_renderer_) {
            GTEST_SKIP() << "Failed to create GLRenderer.";
        }

        renderer_ = engine::render::Renderer::create(gl_renderer_.get(), resource_manager_.get());
        if (!renderer_) {
            GTEST_SKIP() << "Failed to create Renderer.";
        }

        camera_ = std::make_unique<engine::render::Camera>(game_state_->getLogicalSize());
        text_renderer_ = engine::render::TextRenderer::create(gl_renderer_.get(), resource_manager_.get(), &dispatcher_);
        if (!text_renderer_) {
            GTEST_SKIP() << "Failed to create TextRenderer.";
        }

        time_ = std::make_unique<engine::core::Time>();
        main_thread_command_queue_ = std::make_unique<engine::async::MainThreadCommandQueue>();
#ifdef TF_ENABLE_DEBUG_UI
        debug_ui_manager_ = std::make_unique<engine::debug::DebugUIManager>();
#endif
        context_ = engine::core::Context::create(dispatcher_,
                                                 *input_manager_,
                                                 *gl_renderer_,
                                                 *renderer_,
                                                 *camera_,
                                                 *text_renderer_,
                                                 *resource_manager_,
                                                 auto_tile_library_,
                                                 *ui_preset_manager_,
                                                 *audio_player_,
                                                 *game_state_,
                                                 *time_,
                                                 *main_thread_command_queue_,
#ifdef TF_ENABLE_DEBUG_UI
                                                 *debug_ui_manager_,
#endif
                                                 spatial_index_manager_);
        if (!context_) {
            GTEST_SKIP() << "Failed to create Context.";
        }

        scene_ = std::make_unique<TestScene>(*context_);
        ASSERT_TRUE(scene_->init());

        const entt::id_type initial_map_id = entt::hashed_string{"home_exterior"}.value();
        ASSERT_TRUE(world_state_.loadFromWorldFile("assets/maps/farm-rpg.world", initial_map_id, "assets/maps/"));

        entity_factory_ = std::make_unique<game::factory::EntityFactory>(
            scene_->getRegistry(), blueprint_manager_, &context_->getSpatialIndexManager(), &context_->getAutoTileLibrary());
        map_manager_ = std::make_unique<game::world::MapManager>(
            *scene_, *context_, scene_->getRegistry(), world_state_, *entity_factory_, blueprint_manager_);

        game::world::MapLoadingSettings settings{};
        settings.preload_mode = game::world::MapPreloadMode::All;
        settings.async_preload_enabled = true;
        settings.async_wait_budget_ms = 3;
        settings.async_submit_wait_ms = 1;
        settings.async_command_wait_ms = 8;
        settings.async_worker_count = 2;
        settings.async_queue_capacity = 32;
        map_manager_->setLoadingSettings(settings);
    }

    void TearDown() override {
        map_manager_.reset();
        entity_factory_.reset();
        scene_.reset();
        context_.reset();
#ifdef TF_ENABLE_DEBUG_UI
        debug_ui_manager_.reset();
#endif
        main_thread_command_queue_.reset();
        time_.reset();
        text_renderer_.reset();
        camera_.reset();
        renderer_.reset();
        gl_renderer_.reset();
        audio_player_.reset();
        ui_preset_manager_.reset();
        resource_manager_.reset();
        input_manager_.reset();
        game_state_.reset();

        if (window_ != nullptr) {
            SDL_DestroyWindow(window_);
            window_ = nullptr;
        }

        std::error_code ec;
        std::filesystem::remove(input_config_path_, ec);
        if (!original_working_dir_.empty()) {
            std::filesystem::current_path(original_working_dir_, ec);
        }
    }
};

TEST_F(MapManagerAsyncPreloadTest, InitialStatesAreNotScheduled) {
    ASSERT_FALSE(world_state_.maps().empty());

    EXPECT_EQ(map_manager_->preloadedMapCount(), 0U);
    for (const auto& [map_id, _] : world_state_.maps()) {
        EXPECT_FALSE(map_manager_->isMapPreloaded(map_id));
        const auto state = map_manager_->mapPreloadTaskState(map_id);
        EXPECT_EQ(state, game::world::MapPreloadTaskState::NotScheduled);
    }
}

} // namespace
} // namespace game::world
// NOLINTEND
