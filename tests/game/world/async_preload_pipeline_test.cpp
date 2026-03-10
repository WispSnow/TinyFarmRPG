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
#include <utility>

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
#include "engine/spatial/spatial_index_manager.h"
#include "game/world/async_preload_pipeline.h"
#include "game/world/map_loading_settings.h"
#include "game/world/world_state.h"

namespace game::world {
namespace {

[[nodiscard]] bool initSdlVideoWithDummyFallback(Uint32 flags) {
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
    return SDL_Init(flags);
}

struct WaitResult {
    MapPreloadTaskState state{MapPreloadTaskState::NotScheduled};
    std::size_t drained{0};
};

class AsyncPreloadPipelineTest : public ::testing::Test {
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

    game::world::WorldState world_state_{};
    std::unique_ptr<game::world::AsyncPreloadPipeline> pipeline_{};

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
            GTEST_SKIP() << "Unable to locate runtime assets directory for async preload pipeline test.";
        }

        std::error_code ec;
        std::filesystem::current_path(runtime_root_, ec);
        if (ec) {
            GTEST_SKIP() << "Failed to switch working directory to runtime root.";
        }

        window_ = SDL_CreateWindow("AsyncPreloadPipelineTest", 640, 360, SDL_WINDOW_HIDDEN);
        if (!window_) {
            GTEST_SKIP() << "Failed to create SDL window.";
        }

        game_state_ = engine::core::GameState::create(window_);
        if (!game_state_) {
            GTEST_SKIP() << "Failed to create GameState.";
        }
        game_state_->setWindowSize({640.0F, 360.0F});
        game_state_->setLogicalSize({640.0F, 360.0F});

        input_config_path_ = std::filesystem::temp_directory_path() / "async_preload_pipeline_input.json";
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
        main_thread_command_queue_ = std::make_unique<engine::async::MainThreadCommandQueue>(1);
#ifdef TF_ENABLE_DEBUG_UI
        debug_ui_manager_ = std::make_unique<engine::debug::DebugUIManager>();
#endif
        engine::core::CoreServices core_services{
            dispatcher_, *game_state_, *time_, *input_manager_, *main_thread_command_queue_
        };
        engine::core::RenderServices render_services{
            *gl_renderer_, *renderer_, *camera_, *text_renderer_
        };
        engine::core::ResourceServices resource_services{
            *resource_manager_, auto_tile_library_
        };
        context_ = engine::core::Context::create(
            core_services, render_services, resource_services,
            *audio_player_, spatial_index_manager_
#ifdef TF_ENABLE_DEBUG_UI
            , *debug_ui_manager_
#endif
        );
        if (!context_) {
            GTEST_SKIP() << "Failed to create Context.";
        }

        const entt::id_type initial_map_id = entt::hashed_string{"home_exterior"}.value();
        ASSERT_TRUE(world_state_.loadFromWorldFile("assets/maps/farm-rpg.world", initial_map_id, "assets/maps/"));

        game::world::MapLoadingSettings settings{};
        settings.preload_mode = game::world::MapPreloadMode::All;
        settings.async_preload_enabled = true;
        settings.async_wait_budget_ms = 3;
        settings.async_submit_wait_ms = 1;
        settings.async_command_wait_ms = 8;
        settings.async_worker_count = 1;
        settings.async_queue_capacity = 32;
        pipeline_ = std::make_unique<game::world::AsyncPreloadPipeline>(*context_, settings);
    }

    void TearDown() override {
        pipeline_.reset();
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

    [[nodiscard]] std::pair<entt::id_type, std::string> firstMap() const {
        if (world_state_.maps().empty()) {
            return {entt::id_type{0}, {}};
        }
        const auto& [map_id, state] = *world_state_.maps().begin();
        return {map_id, state.info.file_path};
    }

    [[nodiscard]] WaitResult waitForTerminal(entt::id_type map_id,
                                             std::chrono::milliseconds timeout,
                                             bool drain_commands) {
        WaitResult result{};
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            if (drain_commands) {
                result.drained += context_->getMainThreadCommandQueue().drain();
            }

            result.state = pipeline_->getTaskState(map_id);
            if (result.state != MapPreloadTaskState::NotScheduled &&
                result.state != MapPreloadTaskState::Running) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return result;
    }
};

TEST_F(AsyncPreloadPipelineTest, ScheduleAndDrainReachesTerminalState) {
    const auto [map_id, level_path] = firstMap();
    ASSERT_NE(map_id, entt::id_type{0});
    ASSERT_TRUE(pipeline_->schedule(map_id, level_path));

    const auto result = waitForTerminal(map_id, std::chrono::seconds(5), true);
    EXPECT_NE(result.state, MapPreloadTaskState::NotScheduled);
    EXPECT_NE(result.state, MapPreloadTaskState::Running);

    if (result.state == MapPreloadTaskState::Ready) {
        pipeline_->markAppliedIfReady(map_id);
        EXPECT_EQ(pipeline_->getTaskState(map_id), MapPreloadTaskState::Applied);
    }
}

TEST_F(AsyncPreloadPipelineTest, QueueFullTimesOutAndMarksFailed) {
    auto settings = pipeline_->loadingSettings();
    settings.async_command_wait_ms = 1;
    settings.async_worker_count = 1;
    pipeline_->setLoadingSettings(settings);

    auto& command_queue = context_->getMainThreadCommandQueue();
    ASSERT_TRUE(command_queue.enqueue([]() {}));
    ASSERT_EQ(command_queue.size(), 1U);

    const auto [map_id, level_path] = firstMap();
    ASSERT_NE(map_id, entt::id_type{0});
    ASSERT_TRUE(pipeline_->schedule(map_id, level_path));

    const auto result = waitForTerminal(map_id, std::chrono::seconds(5), false);
    EXPECT_EQ(result.state, MapPreloadTaskState::Failed);

    (void)command_queue.drain();
}

TEST_F(AsyncPreloadPipelineTest, RescheduleSameMapDropsStaleGenerationWork) {
    auto settings = pipeline_->loadingSettings();
    settings.async_command_wait_ms = 100;
    settings.async_worker_count = 1;
    pipeline_->setLoadingSettings(settings);

    const auto [map_id, level_path] = firstMap();
    ASSERT_NE(map_id, entt::id_type{0});
    ASSERT_TRUE(pipeline_->schedule(map_id, level_path));
    ASSERT_TRUE(pipeline_->schedule(map_id, level_path));

    const auto result = waitForTerminal(map_id, std::chrono::seconds(5), true);
    EXPECT_NE(result.state, MapPreloadTaskState::NotScheduled);
    EXPECT_NE(result.state, MapPreloadTaskState::Running);
    EXPECT_LE(result.drained, 1U);
}

TEST_F(AsyncPreloadPipelineTest, DestroyPipelineWhileTaskRunningExitsSafely) {
    auto settings = pipeline_->loadingSettings();
    settings.async_command_wait_ms = 200;
    settings.async_worker_count = 1;
    pipeline_->setLoadingSettings(settings);

    auto& command_queue = context_->getMainThreadCommandQueue();
    ASSERT_TRUE(command_queue.enqueue([]() {}));
    ASSERT_EQ(command_queue.size(), 1U);

    const auto [map_id, level_path] = firstMap();
    ASSERT_NE(map_id, entt::id_type{0});
    ASSERT_TRUE(pipeline_->schedule(map_id, level_path));
    EXPECT_EQ(pipeline_->getTaskState(map_id), MapPreloadTaskState::Running);

    const auto start = std::chrono::steady_clock::now();
    pipeline_.reset();
    const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();

    EXPECT_LT(elapsed_ms, 3000);

    (void)command_queue.drain();
}

} // namespace
} // namespace game::world
// NOLINTEND
