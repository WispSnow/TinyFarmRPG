// NOLINTBEGIN
#include <gtest/gtest.h>

#include <SDL3/SDL.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include <entt/core/hashed_string.hpp>
#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>

#include "engine/component/name_component.h"
#include "engine/component/transform_component.h"
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
#include "engine/utils/events.h"

#include "game/component/merchant_component.h"
#include "game/component/npc_component.h"
#include "game/component/scripted_interaction_component.h"
#include "game/component/tags.h"
#include "game/data/item_catalog.h"
#include "game/data/shop_catalog.h"
#include "game/defs/commands.h"
#include "game/defs/events.h"
#include "game/domain/inventory_domain_service.h"
#include "game/domain/shop_transaction_service.h"
#include "game/scene/shop_menu_scene.h"
#include "game/system/shop_interaction_system.h"

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

namespace {

[[nodiscard]] bool initSdlVideoWithDummyFallback(const Uint32 flags) {
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
    return SDL_Init(flags);
}

[[nodiscard]] game::data::ItemCatalog loadProjectItemCatalog() {
    game::data::ItemCatalog catalog;
    const auto config_path = (std::filesystem::path{PROJECT_SOURCE_DIR} / "assets/data/item_config.json").lexically_normal();
    EXPECT_TRUE(catalog.loadItemConfig(config_path.string()));
    return catalog;
}

[[nodiscard]] game::data::ShopCatalog loadProjectShopCatalog() {
    game::data::ShopCatalog catalog;
    const auto root = std::filesystem::path{PROJECT_SOURCE_DIR};
    EXPECT_TRUE(catalog.loadFromFile((root / "assets/data/shops.json").string()));
    auto item_catalog = loadProjectItemCatalog();
    std::string validation_error{};
    EXPECT_TRUE(catalog.validateReferences(&item_catalog, validation_error)) << validation_error;
    return catalog;
}

struct PushSceneCapture {
    int count{0};
    std::string scene_name{};
    bool saw_shop_menu_scene{false};
    std::unique_ptr<engine::scene::Scene> captured_scene{};

    void onEvent(engine::utils::PushSceneEvent& event) {
        ++count;
        if (!event.scene) {
            return;
        }
        scene_name = std::string(event.scene->getName());
        saw_shop_menu_scene = dynamic_cast<game::scene::ShopMenuScene*>(event.scene.get()) != nullptr;
        captured_scene = std::move(event.scene);
    }
};

struct PopSceneCapture {
    int count{0};

    void onEvent(engine::utils::PopSceneEvent&) {
        ++count;
    }
};

struct DialogueCapture {
    std::vector<game::defs::DialogueShowEvent> shows{};
    int hides{0};

    void onShow(const game::defs::DialogueShowEvent& event) {
        shows.push_back(event);
    }

    void onHide(const game::defs::DialogueHideEvent&) {
        ++hides;
    }
};

class ShopInteractionSystemTest : public ::testing::Test {
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
            GTEST_SKIP() << "Unable to locate runtime assets directory.";
        }

        std::error_code ec;
        std::filesystem::current_path(runtime_root_, ec);
        if (ec) {
            GTEST_SKIP() << "Failed to switch working directory to runtime root.";
        }

        window_ = SDL_CreateWindow("ShopInteractionSystemTest", 640, 360, SDL_WINDOW_HIDDEN);
        if (!window_) {
            GTEST_SKIP() << "Failed to create SDL window.";
        }

        game_state_ = engine::core::GameState::create(window_);
        if (!game_state_) {
            GTEST_SKIP() << "Failed to create GameState.";
        }
        game_state_->setState(engine::core::State::Playing);
        game_state_->setWindowSize({640.0F, 360.0F});
        game_state_->setLogicalSize({640.0F, 360.0F});

        input_config_path_ = std::filesystem::temp_directory_path() /
                             ("shop_interaction_input_" +
                              std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".json");
        std::ofstream input_config(input_config_path_);
        ASSERT_TRUE(input_config.is_open());
        input_config << R"({"input_mappings":{}})";
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
            core_services,
            render_services,
            resource_services,
            ui_services,
            *audio_player_,
            spatial_index_manager_
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

        if (window_ != nullptr) {
            SDL_DestroyWindow(window_);
            window_ = nullptr;
        }

        std::error_code ec;
        std::filesystem::remove(input_config_path_, ec);
        std::filesystem::current_path(original_working_dir_, ec);
    }
};

} // namespace

namespace game::system {

TEST_F(ShopInteractionSystemTest, ValidMerchantShowsGreetingBeforePushingShopMenuScene) {
    entt::registry registry;
    auto item_catalog = loadProjectItemCatalog();
    auto shop_catalog = loadProjectShopCatalog();
    game::domain::InventoryDomainService inventory_domain_service(registry, dispatcher_, item_catalog);
    game::domain::ShopTransactionService shop_transaction_service(
        registry,
        item_catalog,
        shop_catalog,
        inventory_domain_service);
    ShopInteractionSystem system(registry, *context_, shop_catalog, item_catalog, shop_transaction_service);

    const entt::entity player = registry.create();
    registry.emplace<engine::component::TransformComponent>(player, glm::vec2{0.0F, 0.0F});
    const entt::entity merchant = registry.create();
    registry.emplace<engine::component::TransformComponent>(merchant, glm::vec2{0.0F, 0.0F});
    registry.emplace<game::component::MerchantComponent>(
        merchant,
        game::component::MerchantComponent{
            .shop_id_ = "shop.village.general",
            .shop_id_hash_ = entt::hashed_string{"shop.village.general"}.value()});
    registry.emplace<engine::component::NameComponent>(
        merchant,
        engine::component::NameComponent{entt::hashed_string{"merchant"}.value(), "Josh"});

    PushSceneCapture capture{};
    DialogueCapture dialogue_capture{};
    dispatcher_.sink<engine::utils::PushSceneEvent>().connect<&PushSceneCapture::onEvent>(&capture);
    dispatcher_.sink<game::defs::DialogueShowEvent>().connect<&DialogueCapture::onShow>(&dialogue_capture);
    dispatcher_.sink<game::defs::DialogueHideEvent>().connect<&DialogueCapture::onHide>(&dialogue_capture);

    dispatcher_.trigger(game::defs::InteractCommand{player, merchant});

    EXPECT_EQ(capture.count, 0);
    ASSERT_EQ(dialogue_capture.shows.size(), 1U);
    EXPECT_EQ(dialogue_capture.shows.front().speaker, "Josh");
    EXPECT_EQ(dialogue_capture.shows.front().text, "Welcome to the shop");

    auto& dialogue = registry.get<game::component::DialogueComponent>(merchant);
    EXPECT_TRUE(dialogue.active_);
    dialogue.cooldown_timer_ = 0.0F;

    dispatcher_.trigger(game::defs::InteractCommand{player, merchant});

    EXPECT_EQ(capture.count, 1);
    EXPECT_EQ(capture.scene_name, "ShopMenu");
    EXPECT_TRUE(capture.saw_shop_menu_scene);
    ASSERT_NE(capture.captured_scene, nullptr);
    EXPECT_FALSE(dialogue.active_);
    EXPECT_EQ(dialogue_capture.hides, 1);
}

TEST_F(ShopInteractionSystemTest, OpenShopCommandPushesSelectedShopMenuScene) {
    entt::registry registry;
    auto item_catalog = loadProjectItemCatalog();
    auto shop_catalog = loadProjectShopCatalog();
    game::domain::InventoryDomainService inventory_domain_service(registry, dispatcher_, item_catalog);
    game::domain::ShopTransactionService shop_transaction_service(
        registry,
        item_catalog,
        shop_catalog,
        inventory_domain_service);
    ShopInteractionSystem system(registry, *context_, shop_catalog, item_catalog, shop_transaction_service);

    const entt::entity player = registry.create();
    const entt::entity merchant = registry.create();

    PushSceneCapture capture{};
    dispatcher_.sink<engine::utils::PushSceneEvent>().connect<&PushSceneCapture::onEvent>(&capture);

    dispatcher_.trigger(game::defs::OpenShopCommand{
        .player = player,
        .merchant = merchant,
        .shop_id_hash = entt::hashed_string{"shop.village.general"}.value(),
        .shop_id = "shop.village.general"});

    EXPECT_EQ(capture.count, 1);
    EXPECT_EQ(capture.scene_name, "ShopMenu");
    EXPECT_TRUE(capture.saw_shop_menu_scene);
}

TEST_F(ShopInteractionSystemTest, ScriptedMerchantInteractIsIgnoredByCppSystem) {
    entt::registry registry;
    auto item_catalog = loadProjectItemCatalog();
    auto shop_catalog = loadProjectShopCatalog();
    game::domain::InventoryDomainService inventory_domain_service(registry, dispatcher_, item_catalog);
    game::domain::ShopTransactionService shop_transaction_service(
        registry,
        item_catalog,
        shop_catalog,
        inventory_domain_service);
    ShopInteractionSystem system(registry, *context_, shop_catalog, item_catalog, shop_transaction_service);

    const entt::entity player = registry.create();
    registry.emplace<engine::component::TransformComponent>(player, glm::vec2{0.0F, 0.0F});

    const entt::entity merchant = registry.create();
    registry.emplace<engine::component::TransformComponent>(merchant, glm::vec2{0.0F, 0.0F});
    registry.emplace<game::component::MerchantComponent>(
        merchant,
        game::component::MerchantComponent{
            .shop_id_ = "shop.village.general",
            .shop_id_hash_ = entt::hashed_string{"shop.village.general"}.value()});
    registry.emplace<game::component::ScriptedInteractionComponent>(merchant);

    PushSceneCapture capture{};
    DialogueCapture dialogue_capture{};
    dispatcher_.sink<engine::utils::PushSceneEvent>().connect<&PushSceneCapture::onEvent>(&capture);
    dispatcher_.sink<game::defs::DialogueShowEvent>().connect<&DialogueCapture::onShow>(&dialogue_capture);

    dispatcher_.trigger(game::defs::InteractCommand{player, merchant});

    EXPECT_EQ(capture.count, 0);
    EXPECT_TRUE(dialogue_capture.shows.empty());
    EXPECT_FALSE(registry.any_of<game::component::DialogueComponent>(merchant));
}

TEST_F(ShopInteractionSystemTest, ActiveMerchantGreetingClosesWhenPlayerLeavesRange) {
    entt::registry registry;
    auto item_catalog = loadProjectItemCatalog();
    auto shop_catalog = loadProjectShopCatalog();
    game::domain::InventoryDomainService inventory_domain_service(registry, dispatcher_, item_catalog);
    game::domain::ShopTransactionService shop_transaction_service(
        registry,
        item_catalog,
        shop_catalog,
        inventory_domain_service);
    ShopInteractionSystem system(registry, *context_, shop_catalog, item_catalog, shop_transaction_service);

    const entt::entity player = registry.create();
    registry.emplace<game::component::PlayerTag>(player);
    registry.emplace<engine::component::TransformComponent>(player, glm::vec2{0.0F, 0.0F});

    const entt::entity merchant = registry.create();
    registry.emplace<engine::component::TransformComponent>(merchant, glm::vec2{0.0F, 0.0F});
    registry.emplace<game::component::MerchantComponent>(
        merchant,
        game::component::MerchantComponent{
            .shop_id_ = "shop.village.general",
            .shop_id_hash_ = entt::hashed_string{"shop.village.general"}.value()});

    PushSceneCapture capture{};
    DialogueCapture dialogue_capture{};
    dispatcher_.sink<engine::utils::PushSceneEvent>().connect<&PushSceneCapture::onEvent>(&capture);
    dispatcher_.sink<game::defs::DialogueHideEvent>().connect<&DialogueCapture::onHide>(&dialogue_capture);

    dispatcher_.trigger(game::defs::InteractCommand{player, merchant});
    auto& dialogue = registry.get<game::component::DialogueComponent>(merchant);
    ASSERT_TRUE(dialogue.active_);

    registry.get<engine::component::TransformComponent>(player).position_ = {96.0F, 0.0F};
    system.update(0.016F);

    EXPECT_FALSE(dialogue.active_);
    EXPECT_EQ(dialogue_capture.hides, 1);
    EXPECT_EQ(capture.count, 0);
}

TEST_F(ShopInteractionSystemTest, ActiveMerchantInteractOutOfRangeClosesGreetingInsteadOfOpeningShop) {
    entt::registry registry;
    auto item_catalog = loadProjectItemCatalog();
    auto shop_catalog = loadProjectShopCatalog();
    game::domain::InventoryDomainService inventory_domain_service(registry, dispatcher_, item_catalog);
    game::domain::ShopTransactionService shop_transaction_service(
        registry,
        item_catalog,
        shop_catalog,
        inventory_domain_service);
    ShopInteractionSystem system(registry, *context_, shop_catalog, item_catalog, shop_transaction_service);

    const entt::entity player = registry.create();
    registry.emplace<engine::component::TransformComponent>(player, glm::vec2{0.0F, 0.0F});

    const entt::entity merchant = registry.create();
    registry.emplace<engine::component::TransformComponent>(merchant, glm::vec2{0.0F, 0.0F});
    registry.emplace<game::component::MerchantComponent>(
        merchant,
        game::component::MerchantComponent{
            .shop_id_ = "shop.village.general",
            .shop_id_hash_ = entt::hashed_string{"shop.village.general"}.value()});

    PushSceneCapture capture{};
    DialogueCapture dialogue_capture{};
    dispatcher_.sink<engine::utils::PushSceneEvent>().connect<&PushSceneCapture::onEvent>(&capture);
    dispatcher_.sink<game::defs::DialogueHideEvent>().connect<&DialogueCapture::onHide>(&dialogue_capture);

    dispatcher_.trigger(game::defs::InteractCommand{player, merchant});
    auto& dialogue = registry.get<game::component::DialogueComponent>(merchant);
    ASSERT_TRUE(dialogue.active_);
    dialogue.cooldown_timer_ = 0.0F;
    registry.get<engine::component::TransformComponent>(player).position_ = {96.0F, 0.0F};

    dispatcher_.trigger(game::defs::InteractCommand{player, merchant});

    EXPECT_FALSE(dialogue.active_);
    EXPECT_EQ(dialogue_capture.hides, 1);
    EXPECT_EQ(capture.count, 0);
}

TEST_F(ShopInteractionSystemTest, PushedShopMenuSceneRemainsClosableAfterMerchantInteraction) {
    entt::registry registry;
    auto item_catalog = loadProjectItemCatalog();
    auto shop_catalog = loadProjectShopCatalog();
    game::domain::InventoryDomainService inventory_domain_service(registry, dispatcher_, item_catalog);
    game::domain::ShopTransactionService shop_transaction_service(
        registry,
        item_catalog,
        shop_catalog,
        inventory_domain_service);
    ShopInteractionSystem system(registry, *context_, shop_catalog, item_catalog, shop_transaction_service);

    const entt::entity player = registry.create();
    registry.emplace<engine::component::TransformComponent>(player, glm::vec2{0.0F, 0.0F});
    const entt::entity merchant = registry.create();
    registry.emplace<engine::component::TransformComponent>(merchant, glm::vec2{0.0F, 0.0F});
    registry.emplace<game::component::MerchantComponent>(
        merchant,
        game::component::MerchantComponent{
            .shop_id_ = "shop.village.general",
            .shop_id_hash_ = entt::hashed_string{"shop.village.general"}.value()});

    PushSceneCapture capture{};
    PopSceneCapture pop_capture{};
    dispatcher_.sink<engine::utils::PushSceneEvent>().connect<&PushSceneCapture::onEvent>(&capture);
    dispatcher_.sink<engine::utils::PopSceneEvent>().connect<&PopSceneCapture::onEvent>(&pop_capture);

    dispatcher_.trigger(game::defs::InteractCommand{player, merchant});
    registry.get<game::component::DialogueComponent>(merchant).cooldown_timer_ = 0.0F;
    dispatcher_.trigger(game::defs::InteractCommand{player, merchant});

    ASSERT_NE(capture.captured_scene, nullptr);
    EXPECT_EQ(capture.captured_scene->getName(), "ShopMenu");
    capture.captured_scene->requestPopScene();
    EXPECT_EQ(pop_capture.count, 1);
}

TEST_F(ShopInteractionSystemTest, InvalidShopIdDoesNotPushScene) {
    entt::registry registry;
    auto item_catalog = loadProjectItemCatalog();
    auto shop_catalog = loadProjectShopCatalog();
    game::domain::InventoryDomainService inventory_domain_service(registry, dispatcher_, item_catalog);
    game::domain::ShopTransactionService shop_transaction_service(
        registry,
        item_catalog,
        shop_catalog,
        inventory_domain_service);
    ShopInteractionSystem system(registry, *context_, shop_catalog, item_catalog, shop_transaction_service);

    const entt::entity player = registry.create();
    const entt::entity merchant = registry.create();
    registry.emplace<game::component::MerchantComponent>(
        merchant,
        game::component::MerchantComponent{
            .shop_id_ = "shop.missing",
            .shop_id_hash_ = entt::hashed_string{"shop.missing"}.value()});

    PushSceneCapture capture{};
    dispatcher_.sink<engine::utils::PushSceneEvent>().connect<&PushSceneCapture::onEvent>(&capture);

    dispatcher_.trigger(game::defs::InteractCommand{player, merchant});

    EXPECT_EQ(capture.count, 0);
}

TEST_F(ShopInteractionSystemTest, PausedStateDoesNotPushScene) {
    entt::registry registry;
    auto item_catalog = loadProjectItemCatalog();
    auto shop_catalog = loadProjectShopCatalog();
    game::domain::InventoryDomainService inventory_domain_service(registry, dispatcher_, item_catalog);
    game::domain::ShopTransactionService shop_transaction_service(
        registry,
        item_catalog,
        shop_catalog,
        inventory_domain_service);
    ShopInteractionSystem system(registry, *context_, shop_catalog, item_catalog, shop_transaction_service);

    const entt::entity player = registry.create();
    const entt::entity merchant = registry.create();
    registry.emplace<game::component::MerchantComponent>(
        merchant,
        game::component::MerchantComponent{
            .shop_id_ = "shop.village.general",
            .shop_id_hash_ = entt::hashed_string{"shop.village.general"}.value()});

    PushSceneCapture capture{};
    dispatcher_.sink<engine::utils::PushSceneEvent>().connect<&PushSceneCapture::onEvent>(&capture);

    context_->getGameState().setState(engine::core::State::Paused);
    dispatcher_.trigger(game::defs::InteractCommand{player, merchant});

    EXPECT_EQ(capture.count, 0);
}

TEST_F(ShopInteractionSystemTest, NonMerchantTargetDoesNotPushScene) {
    entt::registry registry;
    auto item_catalog = loadProjectItemCatalog();
    auto shop_catalog = loadProjectShopCatalog();
    game::domain::InventoryDomainService inventory_domain_service(registry, dispatcher_, item_catalog);
    game::domain::ShopTransactionService shop_transaction_service(
        registry,
        item_catalog,
        shop_catalog,
        inventory_domain_service);
    ShopInteractionSystem system(registry, *context_, shop_catalog, item_catalog, shop_transaction_service);

    const entt::entity player = registry.create();
    const entt::entity npc = registry.create();

    PushSceneCapture capture{};
    dispatcher_.sink<engine::utils::PushSceneEvent>().connect<&PushSceneCapture::onEvent>(&capture);

    dispatcher_.trigger(game::defs::InteractCommand{player, npc});

    EXPECT_EQ(capture.count, 0);
}

} // namespace game::system
// NOLINTEND
