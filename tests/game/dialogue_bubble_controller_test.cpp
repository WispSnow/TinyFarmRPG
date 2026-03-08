#include <gtest/gtest.h>

#include <SDL3/SDL.h>
#include <entt/core/hashed_string.hpp>
#include <entt/signal/dispatcher.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

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
#include "engine/ui/ui_label.h"
#include "engine/ui/ui_manager.h"
#include "engine/ui/ui_preset_manager.h"
#include "game/defs/events.h"
#include "game/ui/dialogue_bubble_controller.h"
#include "game/ui/dialogue_bubble_view.h"

namespace game::ui {
namespace {

using namespace entt::literals;

constexpr float kEpsilon = 0.001F;

[[nodiscard]] bool initSdlVideoWithDummyFallback(Uint32 flags) {
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
    return SDL_Init(flags);
}

void expectVec2Near(const glm::vec2& actual, const glm::vec2& expected) {
    EXPECT_NEAR(actual.x, expected.x, kEpsilon);
    EXPECT_NEAR(actual.y, expected.y, kEpsilon);
}

engine::ui::UILabel* findFirstLabel(engine::ui::UIElement& node) {
    if (auto* label = dynamic_cast<engine::ui::UILabel*>(&node)) {
        return label;
    }
    for (const auto& child : node.getChildren()) {
        if (!child) {
            continue;
        }
        if (auto* found = findFirstLabel(*child)) {
            return found;
        }
    }
    return nullptr;
}

class DialogueBubbleControllerTest : public ::testing::Test {
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

        window_ = SDL_CreateWindow("DialogueBubbleControllerTest", 320, 180, SDL_WINDOW_HIDDEN);
        if (!window_) {
            GTEST_SKIP() << "Failed to create SDL window.";
        }

        game_state_ = engine::core::GameState::create(window_);
        if (!game_state_) {
            GTEST_SKIP() << "Failed to create GameState.";
        }
        game_state_->setWindowSize({320.0F, 180.0F});
        game_state_->setLogicalSize({320.0F, 180.0F});

        const auto timestamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        input_config_path_ =
            std::filesystem::temp_directory_path() / ("dialogue_bubble_controller_input_" + std::to_string(timestamp) + ".json");
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
        engine::core::CoreServices core_services{
            dispatcher_, *game_state_, *time_, *input_manager_, *main_thread_command_queue_
        };
        engine::core::RenderServices render_services{
            *gl_renderer_, *renderer_, *camera_, *text_renderer_
        };
        engine::core::ResourceServices resource_services{
            *resource_manager_, auto_tile_library_, *ui_preset_manager_
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

TEST_F(DialogueBubbleControllerTest, ShowMoveHideEventsDriveBubbleState) {
    if (!context_->getGLRenderer().getRmlUILayer()) {
        GTEST_SKIP() << "RmlUILayer not available in dialogue bubble test environment.";
    }

    engine::ui::UIManager ui_manager(*context_, game_state_->getLogicalSize());
    game::ui::DialogueBubbleController controller(dispatcher_);

    constexpr entt::id_type bubble_id = "dialogue_bubble_test_ch1"_hs;
    auto bubble = std::make_unique<game::ui::DialogueBubbleView>(*context_, *text_renderer_, 0);
    auto* bubble_ptr = bubble.get();
    bubble->setId(bubble_id);
    ui_manager.addElement(std::move(bubble));
    controller.registerBubble(1, bubble_ptr, {3.0F, -7.0F});

    game::defs::DialogueShowEvent show_evt{};
    show_evt.channel = 1;
    show_evt.speaker = "NPC";
    show_evt.text = "Hello";
    show_evt.world_position = {10.0F, 20.0F};
    dispatcher_.trigger(show_evt);

    ASSERT_TRUE(bubble_ptr->isVisible());
    EXPECT_EQ(bubble_ptr->getPositioningMode(), engine::ui::PositioningMode::WorldAnchor);
    expectVec2Near(bubble_ptr->getWorldAnchor(), {10.0F, 20.0F});
    expectVec2Near(bubble_ptr->getPreviousWorldAnchor(), {10.0F, 20.0F});
    expectVec2Near(bubble_ptr->getWorldAnchorOffset(), {3.0F, -7.0F});

    auto* label = findFirstLabel(*bubble_ptr);
    ASSERT_NE(label, nullptr);
    EXPECT_EQ(label->getText(), "NPC: \nHello");

    game::defs::DialogueMoveEvent move_evt{};
    move_evt.channel = 1;
    move_evt.world_position = {24.0F, 42.0F};
    dispatcher_.trigger(move_evt);

    expectVec2Near(bubble_ptr->getPreviousWorldAnchor(), {10.0F, 20.0F});
    expectVec2Near(bubble_ptr->getWorldAnchor(), {24.0F, 42.0F});

    game::defs::DialogueHideEvent hide_evt{};
    hide_evt.channel = 1;
    dispatcher_.enqueue(hide_evt);
    dispatcher_.update();

    EXPECT_FALSE(bubble_ptr->isVisible());
    EXPECT_EQ(bubble_ptr->getPositioningMode(), engine::ui::PositioningMode::Screen);
    expectVec2Near(bubble_ptr->getWorldAnchor(), {0.0F, 0.0F});
}

TEST_F(DialogueBubbleControllerTest, UnregisterStopsRoutingAndReregisterRecovers) {
    if (!context_->getGLRenderer().getRmlUILayer()) {
        GTEST_SKIP() << "RmlUILayer not available in dialogue bubble test environment.";
    }

    engine::ui::UIManager ui_manager(*context_, game_state_->getLogicalSize());
    game::ui::DialogueBubbleController controller(dispatcher_);

    constexpr entt::id_type bubble_id = "dialogue_bubble_test_ch1"_hs;
    auto bubble = std::make_unique<game::ui::DialogueBubbleView>(*context_, *text_renderer_, 0);
    auto* bubble_ptr = bubble.get();
    bubble->setId(bubble_id);
    ui_manager.addElement(std::move(bubble));
    controller.registerBubble(1, bubble_ptr);

    game::defs::DialogueShowEvent show_evt{};
    show_evt.channel = 1;
    show_evt.text = "Routed bubble";
    show_evt.world_position = {16.0F, 32.0F};
    dispatcher_.trigger(show_evt);
    EXPECT_TRUE(bubble_ptr->isVisible());

    controller.unregisterBubble(1);
    bubble_ptr->setVisible(false);
    dispatcher_.trigger(show_evt);
    EXPECT_FALSE(bubble_ptr->isVisible());

    auto recreated = std::make_unique<game::ui::DialogueBubbleView>(*context_, *text_renderer_, 0);
    auto* recreated_ptr = recreated.get();
    recreated->setId(bubble_id);
    ui_manager.addElement(std::move(recreated));

    controller.registerBubble(1, recreated_ptr);
    dispatcher_.trigger(show_evt);
    EXPECT_TRUE(recreated_ptr->isVisible());
    EXPECT_EQ(recreated_ptr->getPositioningMode(), engine::ui::PositioningMode::WorldAnchor);
}

} // namespace
} // namespace game::ui
