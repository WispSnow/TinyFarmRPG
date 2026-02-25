#include <gtest/gtest.h>

#include <SDL3/SDL.h>
#include <entt/signal/dispatcher.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>

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
#include "engine/ui/ui_element.h"
#include "engine/ui/ui_manager.h"
#include "engine/ui/ui_preset_manager.h"

namespace engine::ui {
namespace {

constexpr float kEpsilon = 0.001F;

[[nodiscard]] bool initSdlVideoWithDummyFallback(Uint32 flags) {
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
    return SDL_Init(flags);
}

class WorldAnchorTestElement final : public UIElement {
public:
    using UIElement::UIElement;

    void applyWorldAnchorPositionForTest(glm::vec2 screen_pos) {
        applyWorldAnchorPosition(screen_pos);
    }

    [[nodiscard]] bool isLayoutDirtyForTest() const {
        return layout_dirty_;
    }
};

TEST(UIWorldAnchorTest, SetWorldAnchorFirstCallSnapshotsPreviousAsCurrent) {
    WorldAnchorTestElement element({0.0F, 0.0F}, {10.0F, 10.0F});
    element.setWorldAnchor({12.0F, 34.0F}, {1.0F, 2.0F});

    EXPECT_EQ(element.getPositioningMode(), PositioningMode::WorldAnchor);
    EXPECT_EQ(element.getWorldAnchor(), glm::vec2(12.0F, 34.0F));
    EXPECT_EQ(element.getPreviousWorldAnchor(), glm::vec2(12.0F, 34.0F));
    EXPECT_EQ(element.getWorldAnchorOffset(), glm::vec2(1.0F, 2.0F));
}

TEST(UIWorldAnchorTest, SetWorldAnchorSubsequentCallSnapshotsPreviousValue) {
    WorldAnchorTestElement element({0.0F, 0.0F}, {10.0F, 10.0F});
    element.setWorldAnchor({10.0F, 20.0F});
    element.setWorldAnchor({30.0F, 40.0F}, {3.0F, 4.0F});

    EXPECT_EQ(element.getWorldAnchor(), glm::vec2(30.0F, 40.0F));
    EXPECT_EQ(element.getPreviousWorldAnchor(), glm::vec2(10.0F, 20.0F));
    EXPECT_EQ(element.getWorldAnchorOffset(), glm::vec2(3.0F, 4.0F));
}

TEST(UIWorldAnchorTest, ClearWorldAnchorRestoresScreenModeAndClearsState) {
    WorldAnchorTestElement element({7.0F, 9.0F}, {10.0F, 10.0F});
    element.setWorldAnchor({10.0F, 20.0F}, {3.0F, 4.0F});
    element.clearWorldAnchor();

    EXPECT_EQ(element.getPositioningMode(), PositioningMode::Screen);
    EXPECT_EQ(element.getWorldAnchor(), glm::vec2(0.0F, 0.0F));
    EXPECT_EQ(element.getPreviousWorldAnchor(), glm::vec2(0.0F, 0.0F));
    EXPECT_EQ(element.getWorldAnchorOffset(), glm::vec2(0.0F, 0.0F));
}

TEST(UIWorldAnchorTest, ApplyWorldAnchorPositionConsidersPivot) {
    WorldAnchorTestElement element({0.0F, 0.0F}, {100.0F, 40.0F});
    element.setPivot({0.5F, 1.0F});
    element.applyWorldAnchorPositionForTest({200.0F, 300.0F});

    const glm::vec2 layout_position = element.getLayoutPosition();
    EXPECT_NEAR(layout_position.x, 150.0F, kEpsilon);
    EXPECT_NEAR(layout_position.y, 260.0F, kEpsilon);
}

TEST(UIWorldAnchorTest, ApplyWorldAnchorPositionInvalidatesChildren) {
    WorldAnchorTestElement parent({0.0F, 0.0F}, {20.0F, 20.0F});
    auto child = std::make_unique<WorldAnchorTestElement>(glm::vec2{1.0F, 2.0F}, glm::vec2{3.0F, 4.0F});
    auto* child_ptr = child.get();
    parent.addChild(std::move(child));

    (void)child_ptr->getLayoutPosition();
    EXPECT_FALSE(child_ptr->isLayoutDirtyForTest());

    parent.applyWorldAnchorPositionForTest({10.0F, 10.0F});
    EXPECT_TRUE(child_ptr->isLayoutDirtyForTest());
}

TEST(UIWorldAnchorTest, ApplyWorldAnchorPositionDiffGuardSkipsUnchangedInvalidation) {
    WorldAnchorTestElement parent({0.0F, 0.0F}, {20.0F, 20.0F});
    auto child = std::make_unique<WorldAnchorTestElement>(glm::vec2{1.0F, 2.0F}, glm::vec2{3.0F, 4.0F});
    auto* child_ptr = child.get();
    parent.addChild(std::move(child));

    (void)child_ptr->getLayoutPosition();
    parent.applyWorldAnchorPositionForTest({10.0F, 10.0F});
    (void)child_ptr->getLayoutPosition();
    EXPECT_FALSE(child_ptr->isLayoutDirtyForTest());

    parent.applyWorldAnchorPositionForTest({10.0F, 10.0F});
    EXPECT_FALSE(child_ptr->isLayoutDirtyForTest());
}

TEST(UIWorldAnchorTest, WorldAnchorOnNonRootChildFallsBackToScreenPath) {
    WorldAnchorTestElement root({0.0F, 0.0F}, {100.0F, 100.0F});
    auto parent = std::make_unique<WorldAnchorTestElement>(glm::vec2{10.0F, 20.0F}, glm::vec2{50.0F, 50.0F});
    auto child = std::make_unique<WorldAnchorTestElement>(glm::vec2{3.0F, 4.0F}, glm::vec2{10.0F, 10.0F});
    auto* child_ptr = child.get();
    parent->addChild(std::move(child));
    root.addChild(std::move(parent));

    child_ptr->setWorldAnchor({80.0F, 90.0F});
    const glm::vec2 layout_position = child_ptr->getLayoutPosition();
    EXPECT_EQ(child_ptr->getPositioningMode(), PositioningMode::WorldAnchor);
    EXPECT_NEAR(layout_position.x, 13.0F, kEpsilon);
    EXPECT_NEAR(layout_position.y, 24.0F, kEpsilon);
}

TEST(UIWorldAnchorTest, ClearWorldAnchorUsesExistingScreenPosition) {
    WorldAnchorTestElement root({0.0F, 0.0F}, {100.0F, 100.0F});
    auto child = std::make_unique<WorldAnchorTestElement>(glm::vec2{7.0F, 9.0F}, glm::vec2{10.0F, 10.0F});
    auto* child_ptr = child.get();
    root.addChild(std::move(child));

    child_ptr->setWorldAnchor({50.0F, 60.0F});
    child_ptr->clearWorldAnchor();
    const glm::vec2 layout_position = child_ptr->getLayoutPosition();
    EXPECT_NEAR(layout_position.x, 7.0F, kEpsilon);
    EXPECT_NEAR(layout_position.y, 9.0F, kEpsilon);
}

class UIWorldAnchorRuntimeTest : public ::testing::Test {
protected:
    static inline bool sdl_ready_{false};

    SDL_Window* window_{nullptr};
    std::filesystem::path input_config_path_{};

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

        window_ = SDL_CreateWindow("UIWorldAnchorRuntimeTest", 160, 120, SDL_WINDOW_HIDDEN);
        if (!window_) {
            GTEST_SKIP() << "Failed to create SDL window.";
        }

        game_state_ = engine::core::GameState::create(window_);
        if (!game_state_) {
            GTEST_SKIP() << "Failed to create GameState.";
        }
        game_state_->setWindowSize({160.0F, 120.0F});
        game_state_->setLogicalSize({160.0F, 120.0F});

        const auto timestamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        input_config_path_ =
            std::filesystem::temp_directory_path() / ("ui_world_anchor_input_" + std::to_string(timestamp) + ".json");
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
        camera_->setPosition({0.0F, 0.0F});
        camera_->setZoom(1.0F);

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
    }

    void TearDown() override {
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

        if (window_) {
            SDL_DestroyWindow(window_);
            window_ = nullptr;
        }

        std::error_code ec;
        std::filesystem::remove(input_config_path_, ec);
    }
};

TEST_F(UIWorldAnchorRuntimeTest, ResolveWorldAnchorsInterpolatesPosition) {
    UIManager manager(*context_, game_state_->getLogicalSize());

    auto anchored = std::make_unique<WorldAnchorTestElement>(glm::vec2{0.0F, 0.0F}, glm::vec2{10.0F, 10.0F});
    auto* anchored_ptr = anchored.get();
    manager.addElement(std::move(anchored));

    anchored_ptr->setWorldAnchor({0.0F, 0.0F});
    anchored_ptr->setWorldAnchor({100.0F, 0.0F});
    manager.render(*context_, 0.5F);

    const glm::vec2 layout_position = anchored_ptr->getLayoutPosition();
    EXPECT_NEAR(layout_position.x, 130.0F, kEpsilon);
    EXPECT_NEAR(layout_position.y, 60.0F, kEpsilon);
}

TEST_F(UIWorldAnchorRuntimeTest, ResolveWorldAnchorsIgnoresScreenElements) {
    UIManager manager(*context_, game_state_->getLogicalSize());

    auto screen = std::make_unique<WorldAnchorTestElement>(glm::vec2{12.0F, 34.0F}, glm::vec2{20.0F, 20.0F});
    auto* screen_ptr = screen.get();
    manager.addElement(std::move(screen));

    auto anchored = std::make_unique<WorldAnchorTestElement>(glm::vec2{0.0F, 0.0F}, glm::vec2{10.0F, 10.0F});
    auto* anchored_ptr = anchored.get();
    manager.addElement(std::move(anchored));
    anchored_ptr->setWorldAnchor({100.0F, 40.0F});

    const glm::vec2 before = screen_ptr->getLayoutPosition();
    manager.render(*context_, 0.5F);
    const glm::vec2 after = screen_ptr->getLayoutPosition();

    EXPECT_NEAR(before.x, after.x, kEpsilon);
    EXPECT_NEAR(before.y, after.y, kEpsilon);
}

} // namespace
} // namespace engine::ui

