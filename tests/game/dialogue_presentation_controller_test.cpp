#include <gtest/gtest.h>

#include <SDL3/SDL.h>
#include <entt/core/hashed_string.hpp>
#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>
#include <glm/vec2.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

#include "engine/audio/audio_player.h"
#include "engine/async/main_thread_command_queue.h"
#include "engine/component/name_component.h"
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
#include "game/component/actor_identity_component.h"
#include "game/component/player_identity_component.h"
#include "game/component/tags.h"
#include "game/data/rpg_catalog.h"
#include "game/defs/events.h"
#include "game/runtime/localization_service.h"
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

class FakeDialogueBoxView final : public game::ui::DialogueBoxViewPort {
public:
    bool visible{false};
    std::string speaker{};
    std::string text{};
    std::string portrait_decorator{};

    void setSpeaker(std::string_view next_speaker) override { speaker = next_speaker; }
    void setText(std::string_view next_text) override { text = next_text; }
    void setPortraitDecorator(std::string_view decorator) override { portrait_decorator = decorator; }
    void setVisible(bool next_visible) override { visible = next_visible; }
};

class FakeFloatingNoticeView final : public game::ui::FloatingNoticeViewPort {
public:
    bool visible{false};
    bool has_world_anchor{false};
    std::string text{};
    glm::vec2 world_position{0.0F, 0.0F};
    glm::vec2 screen_offset{0.0F, 0.0F};

    void setText(std::string_view next_text) override { text = next_text; }
    void setVisible(bool next_visible) override { visible = next_visible; }
    void setWorldAnchor(glm::vec2 next_world_position, glm::vec2 next_screen_offset = {0.0F, 0.0F}) override {
        has_world_anchor = true;
        world_position = next_world_position;
        screen_offset = next_screen_offset;
    }
    void clearWorldAnchor() override { has_world_anchor = false; }
};

class FakeHotbarVisibility final : public game::ui::HotbarVisibilityPort {
public:
    bool visible{true};
    int show_calls{0};
    int hide_calls{0};

    [[nodiscard]] bool isVisible() const override { return visible; }
    void show() override {
        visible = true;
        ++show_calls;
    }
    void hide() override {
        visible = false;
        ++hide_calls;
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

[[nodiscard]] std::string i18nManifestPath() {
    return (std::filesystem::path{PROJECT_SOURCE_DIR} / "assets/i18n/languages.json").string();
}

TEST(DialoguePresentationControllerUnitTest, ConversationHidesVisibleHotbarUntilDialogueEnds) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    FakeDialogueBoxView box;
    FakeFloatingNoticeView notice;
    FakeFloatingNoticeView item_notice;
    FakeHotbarVisibility hotbar;
    hotbar.visible = true;

    game::ui::DialoguePresentationController controller(
        dispatcher, registry, &box, &notice, &item_notice, &hotbar, nullptr);

    game::defs::DialogueShowEvent show_evt{};
    show_evt.channel = game::defs::DialogueChannel::Conversation;
    show_evt.speaker = "Lyria";
    show_evt.text = "First line";
    dispatcher.trigger(show_evt);

    ASSERT_TRUE(box.visible);
    EXPECT_FALSE(hotbar.visible);
    EXPECT_EQ(hotbar.hide_calls, 1);
    EXPECT_EQ(hotbar.show_calls, 0);

    show_evt.text = "Second line";
    dispatcher.trigger(show_evt);
    EXPECT_EQ(box.text, "Second line");
    EXPECT_FALSE(hotbar.visible);
    EXPECT_EQ(hotbar.hide_calls, 1);

    game::defs::DialogueHideEvent hide_evt{};
    hide_evt.channel = game::defs::DialogueChannel::Conversation;
    dispatcher.trigger(hide_evt);

    EXPECT_FALSE(box.visible);
    EXPECT_TRUE(hotbar.visible);
    EXPECT_EQ(hotbar.show_calls, 1);
}

TEST(DialoguePresentationControllerUnitTest, ConversationLeavesInitiallyHiddenHotbarHidden) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    FakeDialogueBoxView box;
    FakeFloatingNoticeView notice;
    FakeFloatingNoticeView item_notice;
    FakeHotbarVisibility hotbar;
    hotbar.visible = false;

    game::ui::DialoguePresentationController controller(
        dispatcher, registry, &box, &notice, &item_notice, &hotbar, nullptr);

    game::defs::DialogueShowEvent show_evt{};
    show_evt.channel = game::defs::DialogueChannel::Conversation;
    show_evt.text = "Hidden hotbar should stay hidden";
    dispatcher.trigger(show_evt);

    ASSERT_TRUE(box.visible);
    EXPECT_FALSE(hotbar.visible);
    EXPECT_EQ(hotbar.hide_calls, 0);

    game::defs::DialogueHideEvent hide_evt{};
    hide_evt.channel = game::defs::DialogueChannel::Conversation;
    dispatcher.trigger(hide_evt);

    EXPECT_FALSE(box.visible);
    EXPECT_FALSE(hotbar.visible);
    EXPECT_EQ(hotbar.show_calls, 0);
}

TEST(DialoguePresentationControllerUnitTest, ConversationSpeakerUsesLocalizedCatalogActorName) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    FakeDialogueBoxView box;
    FakeFloatingNoticeView notice;
    FakeFloatingNoticeView item_notice;
    FakeHotbarVisibility hotbar;
    game::data::RpgCatalog catalog;
    ASSERT_TRUE(catalog.loadActors(actorsPath()));
    game::runtime::LocalizationService localization;
    ASSERT_TRUE(localization.loadLanguageIndex(i18nManifestPath()));
    ASSERT_TRUE(localization.setLanguage("zh-Hans"));

    game::ui::DialoguePresentationController controller(
        dispatcher,
        registry,
        &box,
        &notice,
        &item_notice,
        &hotbar,
        &catalog,
        {0.0F, -4.0F},
        {0.0F, -56.0F},
        nullptr,
        &localization);

    game::defs::DialogueShowEvent show_evt{};
    show_evt.channel = game::defs::DialogueChannel::Conversation;
    show_evt.speaker = "Lyria";
    show_evt.text = "Hello";
    show_evt.speaker_actor_id = "actor.lyria";
    show_evt.speaker_actor_id_hash = game::data::RpgCatalog::hashId(show_evt.speaker_actor_id);
    dispatcher.trigger(show_evt);

    EXPECT_EQ(box.speaker, "莉莉娅");
    EXPECT_EQ(box.text, "Hello");
}

TEST(DialoguePresentationControllerUnitTest, ConversationSpeakerUsesLocalizedScriptNpcName) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    FakeDialogueBoxView box;
    FakeFloatingNoticeView notice;
    FakeFloatingNoticeView item_notice;
    FakeHotbarVisibility hotbar;
    game::runtime::LocalizationService localization;
    ASSERT_TRUE(localization.loadLanguageIndex(i18nManifestPath()));
    ASSERT_TRUE(localization.setLanguage("zh-Hans"));

    const entt::entity merchant = registry.create();
    registry.emplace<engine::component::NameComponent>(
        merchant,
        entt::hashed_string{"Josh"}.value(),
        "Josh");
    registry.emplace<game::component::ActorIdentityComponent>(
        merchant,
        game::component::ActorIdentityComponent{
            .actor_id_ = "npc.josh",
            .actor_id_hash_ = entt::hashed_string{"npc.josh"}.value(),
            .blueprint_id_ = "merchant",
        });

    game::ui::DialoguePresentationController controller(
        dispatcher,
        registry,
        &box,
        &notice,
        &item_notice,
        &hotbar,
        nullptr,
        {0.0F, -4.0F},
        {0.0F, -56.0F},
        nullptr,
        &localization);

    game::defs::DialogueShowEvent show_evt{};
    show_evt.target = merchant;
    show_evt.channel = game::defs::DialogueChannel::Conversation;
    show_evt.speaker = "Josh";
    show_evt.text = "Welcome";
    dispatcher.trigger(show_evt);

    EXPECT_EQ(box.speaker, "乔希");
}

TEST(DialoguePresentationControllerUnitTest, NoticeSpeakerUsesCustomPlayerIdentityFromTarget) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    FakeDialogueBoxView box;
    FakeFloatingNoticeView notice;
    FakeFloatingNoticeView item_notice;
    FakeHotbarVisibility hotbar;
    game::data::RpgCatalog catalog;
    ASSERT_TRUE(catalog.loadActors(actorsPath()));
    game::runtime::LocalizationService localization;
    ASSERT_TRUE(localization.loadLanguageIndex(i18nManifestPath()));
    ASSERT_TRUE(localization.setLanguage("zh-Hans"));

    const entt::entity player = registry.create();
    registry.emplace<game::component::PlayerTag>(player);
    registry.emplace<game::component::PlayerIdentityComponent>(
        player,
        game::component::PlayerIdentityComponent{.display_name_ = "Mina"});
    registry.emplace<game::component::ActorIdentityComponent>(
        player,
        game::component::ActorIdentityComponent{
            .actor_id_ = "actor.player",
            .actor_id_hash_ = game::data::RpgCatalog::hashId("actor.player"),
            .blueprint_id_ = "player",
        });

    game::ui::DialoguePresentationController controller(
        dispatcher,
        registry,
        &box,
        &notice,
        &item_notice,
        &hotbar,
        &catalog,
        {0.0F, -4.0F},
        {0.0F, -56.0F},
        nullptr,
        &localization);

    game::defs::DialogueShowEvent show_evt{};
    show_evt.target = player;
    show_evt.channel = game::defs::DialogueChannel::Notice;
    show_evt.text = "今天农场小路很安静。出发前先检查宝箱吧。";
    dispatcher.trigger(show_evt);

    EXPECT_EQ(notice.text, "Mina:\n今天农场小路很安静。出发前先检查宝箱吧。");
}

TEST(DialoguePresentationControllerUnitTest, NoticeAutoHidesAfterDefaultDuration) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    FakeDialogueBoxView box;
    FakeFloatingNoticeView notice;
    FakeFloatingNoticeView item_notice;
    FakeHotbarVisibility hotbar;

    game::ui::DialoguePresentationController controller(
        dispatcher, registry, &box, &notice, &item_notice, &hotbar, nullptr);

    game::defs::DialogueShowEvent show_evt{};
    show_evt.channel = game::defs::DialogueChannel::Notice;
    show_evt.text = "Short notice";
    show_evt.world_position = {12.0F, 24.0F};
    dispatcher.trigger(show_evt);

    ASSERT_TRUE(notice.visible);
    EXPECT_TRUE(notice.has_world_anchor);
    expectVec2Near(notice.world_position, {12.0F, 24.0F});

    controller.update(1.0F);
    EXPECT_TRUE(notice.visible);

    controller.update(1.0F);
    EXPECT_FALSE(notice.visible);
    EXPECT_FALSE(notice.has_world_anchor);
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
        dispatcher_, registry_, box.get(), notice.get(), item_notice.get(), nullptr, &catalog);

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
        dispatcher_, registry_, box.get(), notice.get(), item_notice.get(), nullptr, nullptr, {3.0F, -7.0F});

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
        dispatcher_, registry_, box.get(), notice.get(), item_notice.get(), nullptr, nullptr);

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
        dispatcher_, registry_, box.get(), notice.get(), item_notice.get(), nullptr, nullptr);

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
