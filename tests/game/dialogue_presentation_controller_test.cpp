#include <gtest/gtest.h>

#include <SDL3/SDL.h>
#include <entt/entity/registry.hpp>
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
#include "game/data/rpg_catalog.h"
#include "game/defs/events.h"
#include "game/system/system_helpers.h"
#include "game/ui/dialogue_box_view.h"
#include "game/ui/dialogue_presentation_controller.h"
#include "game/ui/floating_notice_view.h"

namespace game::ui {
namespace {

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

struct HideCapture {
    int count{0};
    game::defs::DialogueChannel last_channel{game::defs::DialogueChannel::Conversation};

    void onHide(const game::defs::DialogueHideEvent& evt) {
        ++count;
        last_channel = evt.channel;
    }
};

class DialoguePresentationControllerTest : public ::testing::Test {
protected:
    static inline bool sdl_ready_{false};

    SDL_Window* window_{nullptr};
    std::filesystem::path input_config_path_{};

    entt::registry registry_{};
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

        window_ = SDL_CreateWindow("DialoguePresentationControllerTest", 320, 180, SDL_WINDOW_HIDDEN);
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
            std::filesystem::temp_directory_path() /
            ("dialogue_presentation_controller_input_" + std::to_string(timestamp) + ".json");
        std::ofstream input_config(input_config_path_);
        ASSERT_TRUE(input_config.is_open());
        input_config << R"({"input_mappings":{"primary_action":["MouseLeft"]}})";
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
            *resource_manager_, auto_tile_library_
        };
        engine::core::UiServices ui_services{};
        context_ = engine::core::Context::create(
            core_services, render_services, resource_services, ui_services,
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

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

[[nodiscard]] std::string actorsPath() {
    return (std::filesystem::path{PROJECT_SOURCE_DIR} / "assets/data/rpg/actors.json").string();
}

TEST_F(DialoguePresentationControllerTest, ConversationShowHideDrivesDialogueBox) {
    if (!context_->getRmlUi()) {
        GTEST_SKIP() << "RmlUiRuntime not available in dialogue presentation test environment.";
    }

    game::data::RpgCatalog catalog;
    ASSERT_TRUE(catalog.loadActors(actorsPath()));
    auto box = std::make_unique<game::ui::DialogueBoxView>(*context_, 1);
    auto notice = std::make_unique<game::ui::FloatingNoticeView>(*context_, 1);
    auto item_notice = std::make_unique<game::ui::FloatingNoticeView>(*context_, 1);
    game::ui::DialoguePresentationController controller(
        dispatcher_, registry_, box.get(), notice.get(), item_notice.get(), &catalog);

    game::defs::DialogueShowEvent show_evt{};
    show_evt.channel = game::defs::DialogueChannel::Conversation;
    show_evt.speaker = "Lyria";
    show_evt.text = "Hello";
    show_evt.speaker_actor_id_hash = game::data::RpgCatalog::hashId("actor.lyria");
    show_evt.world_position = {10.0F, 20.0F};
    dispatcher_.trigger(show_evt);

    ASSERT_TRUE(box->isVisible());
    EXPECT_EQ(box->getSpeaker(), "Lyria");
    EXPECT_EQ(box->getText(), "Hello");
    EXPECT_EQ(box->getPortraitDecorator(), "image(portrait-lyria)");
    EXPECT_FALSE(notice->isVisible());

    game::defs::DialogueMoveEvent move_evt{};
    move_evt.channel = game::defs::DialogueChannel::Conversation;
    move_evt.world_position = {24.0F, 42.0F};
    dispatcher_.trigger(move_evt);
    EXPECT_FALSE(notice->isVisible());

    game::defs::DialogueHideEvent hide_evt{};
    hide_evt.channel = game::defs::DialogueChannel::Conversation;
    dispatcher_.enqueue(hide_evt);
    dispatcher_.update();

    EXPECT_FALSE(box->isVisible());
}

TEST_F(DialoguePresentationControllerTest, NoticeShowMoveHideDrivesFloatingNotice) {
    if (!context_->getRmlUi()) {
        GTEST_SKIP() << "RmlUiRuntime not available in dialogue presentation test environment.";
    }

    auto box = std::make_unique<game::ui::DialogueBoxView>(*context_, 1);
    auto notice = std::make_unique<game::ui::FloatingNoticeView>(*context_, 1);
    auto item_notice = std::make_unique<game::ui::FloatingNoticeView>(*context_, 1);
    game::ui::DialoguePresentationController controller(
        dispatcher_, registry_, box.get(), notice.get(), item_notice.get(), nullptr, {3.0F, -7.0F});

    game::defs::DialogueShowEvent show_evt{};
    show_evt.channel = game::defs::DialogueChannel::Notice;
    show_evt.speaker = "NPC";
    show_evt.text = "Routed notice";
    show_evt.world_position = {16.0F, 32.0F};
    dispatcher_.trigger(show_evt);

    ASSERT_TRUE(notice->isVisible());
    EXPECT_TRUE(notice->hasWorldAnchor());
    expectVec2Near(notice->getWorldAnchor(), {16.0F, 32.0F});
    expectVec2Near(notice->getPreviousWorldAnchor(), {16.0F, 32.0F});
    expectVec2Near(notice->getWorldAnchorOffset(), {3.0F, -7.0F});
    EXPECT_EQ(notice->getText(), "NPC:\nRouted notice");
    EXPECT_FALSE(box->isVisible());

    game::defs::DialogueMoveEvent move_evt{};
    move_evt.channel = game::defs::DialogueChannel::Notice;
    move_evt.world_position = {24.0F, 42.0F};
    dispatcher_.trigger(move_evt);

    expectVec2Near(notice->getPreviousWorldAnchor(), {16.0F, 32.0F});
    expectVec2Near(notice->getWorldAnchor(), {24.0F, 42.0F});

    game::defs::DialogueHideEvent hide_evt{};
    hide_evt.channel = game::defs::DialogueChannel::Notice;
    dispatcher_.enqueue(hide_evt);
    dispatcher_.update();

    EXPECT_FALSE(notice->isVisible());
    EXPECT_FALSE(notice->hasWorldAnchor());
}

TEST_F(DialoguePresentationControllerTest, HideChannelsClearIndependentViews) {
    if (!context_->getRmlUi()) {
        GTEST_SKIP() << "RmlUiRuntime not available in dialogue presentation test environment.";
    }

    auto box = std::make_unique<game::ui::DialogueBoxView>(*context_, 1);
    auto notice = std::make_unique<game::ui::FloatingNoticeView>(*context_, 1);
    auto item_notice = std::make_unique<game::ui::FloatingNoticeView>(*context_, 1);
    game::ui::DialoguePresentationController controller(
        dispatcher_, registry_, box.get(), notice.get(), item_notice.get(), nullptr);

    game::defs::DialogueShowEvent show_evt{};
    show_evt.channel = game::defs::DialogueChannel::Conversation;
    show_evt.text = "Conversation";
    show_evt.world_position = {8.0F, 12.0F};
    dispatcher_.trigger(show_evt);
    show_evt.channel = game::defs::DialogueChannel::Notice;
    show_evt.text = "Notice";
    dispatcher_.trigger(show_evt);

    ASSERT_TRUE(box->isVisible());
    ASSERT_TRUE(notice->isVisible());

    game::defs::DialogueHideEvent hide_evt{};
    hide_evt.channel = game::defs::DialogueChannel::Conversation;
    dispatcher_.enqueue(hide_evt);
    hide_evt.channel = game::defs::DialogueChannel::Notice;
    dispatcher_.enqueue(hide_evt);
    dispatcher_.update();

    EXPECT_FALSE(box->isVisible());
    EXPECT_FALSE(notice->isVisible());
}

TEST_F(DialoguePresentationControllerTest, ImmediateHideDoesNotWaitForDispatcherQueue) {
    if (!context_->getRmlUi()) {
        GTEST_SKIP() << "RmlUiRuntime not available in dialogue presentation test environment.";
    }

    auto box = std::make_unique<game::ui::DialogueBoxView>(*context_, 1);
    auto notice = std::make_unique<game::ui::FloatingNoticeView>(*context_, 1);
    auto item_notice = std::make_unique<game::ui::FloatingNoticeView>(*context_, 1);
    game::ui::DialoguePresentationController controller(
        dispatcher_, registry_, box.get(), notice.get(), item_notice.get(), nullptr);

    game::defs::DialogueShowEvent show_evt{};
    show_evt.channel = game::defs::DialogueChannel::Conversation;
    show_evt.text = "Shop greeting";
    dispatcher_.trigger(show_evt);
    ASSERT_TRUE(box->isVisible());

    game::system::helpers::emitDialogueHideNow(
        dispatcher_,
        game::defs::DialogueChannel::Conversation,
        entt::null);

    EXPECT_FALSE(box->isVisible());
}

TEST(DialogueEventHelpersTest, HideHelperQueuesUntilDispatcherUpdate) {
    entt::dispatcher dispatcher;
    HideCapture capture{};
    dispatcher.sink<game::defs::DialogueHideEvent>().connect<&HideCapture::onHide>(&capture);

    game::system::helpers::emitDialogueHide(
        dispatcher,
        game::defs::DialogueChannel::Notice,
        entt::entity{7});

    EXPECT_EQ(capture.count, 0);

    dispatcher.update();
    ASSERT_EQ(capture.count, 1);
    EXPECT_EQ(capture.last_channel, game::defs::DialogueChannel::Notice);

    game::system::helpers::emitDialogueHideNow(
        dispatcher,
        game::defs::DialogueChannel::ItemNotice,
        entt::entity{7});

    ASSERT_EQ(capture.count, 2);
    EXPECT_EQ(capture.last_channel, game::defs::DialogueChannel::ItemNotice);
}

} // namespace
} // namespace game::ui
