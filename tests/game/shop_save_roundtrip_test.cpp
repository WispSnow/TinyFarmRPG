// NOLINTBEGIN
#include <gtest/gtest.h>

#include <SDL3/SDL.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

#include <entt/core/hashed_string.hpp>
#include <entt/signal/dispatcher.hpp>

#include "engine/audio/audio_player.h"
#include "engine/async/main_thread_command_queue.h"
#include "engine/component/transform_component.h"
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
#include "game/component/hotbar_component.h"
#include "game/component/inventory_component.h"
#include "game/component/player_wallet_component.h"
#include "game/component/quest_log_component.h"
#include "game/component/state_component.h"
#include "game/component/tags.h"
#include "game/data/game_time.h"
#include "game/data/item_catalog.h"
#include "game/data/shop_catalog.h"
#include "game/domain/inventory_domain_service.h"
#include "game/domain/shop_transaction_service.h"
#include "game/factory/blueprint_manager.h"
#include "game/factory/entity_factory.h"
#include "game/save/save_service.h"
#include "game/world/map_loading_settings.h"
#include "game/world/map_manager.h"
#include "game/world/world_state.h"

namespace game::save {
namespace {

using namespace entt::literals;

[[nodiscard]] bool initSdlVideoWithDummyFallback(const Uint32 flags) {
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
    return SDL_Init(flags);
}

[[nodiscard]] game::data::ItemCatalog loadProjectItemCatalog() {
    game::data::ItemCatalog catalog;
    const auto config_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "assets/data/item_config.json").lexically_normal();
    EXPECT_TRUE(catalog.loadItemConfig(config_path.string()));
    return catalog;
}

[[nodiscard]] game::data::ShopCatalog loadProjectShopCatalog() {
    game::data::ShopCatalog catalog;
    const auto config_path = (std::filesystem::path{PROJECT_SOURCE_DIR} / "assets/data/shops.json").lexically_normal();
    EXPECT_TRUE(catalog.loadFromFile(config_path.string()));
    auto item_catalog = loadProjectItemCatalog();
    std::string validation_error{};
    EXPECT_TRUE(catalog.validateReferences(&item_catalog, validation_error)) << validation_error;
    return catalog;
}

class TestScene final : public engine::scene::Scene {
public:
    explicit TestScene(engine::core::Context& context)
        : Scene("ShopSaveRoundtripTestScene", context) {}
};

class ShopSaveRoundtripTest : public ::testing::Test {
protected:
    static inline bool sdl_ready_{false};

    SDL_Window* window_{nullptr};
    std::filesystem::path input_config_path_{};
    std::filesystem::path original_working_dir_{};
    std::filesystem::path runtime_root_{};
    std::filesystem::path temp_dir_{};

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

    std::unique_ptr<TestScene> scene_{};
    game::world::WorldState world_state_{};
    game::factory::BlueprintManager blueprint_manager_{};
    std::unique_ptr<game::factory::EntityFactory> entity_factory_{};
    std::unique_ptr<game::world::MapManager> map_manager_{};
    std::unique_ptr<SaveService> save_service_{};

    static void SetUpTestSuite() {
        sdl_ready_ = initSdlVideoWithDummyFallback(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
    }

    static void TearDownTestSuite() {
        if (sdl_ready_) {
            SDL_Quit();
        }
    }

    [[nodiscard]] std::filesystem::path tempFilePath(std::string_view filename) const {
        return (temp_dir_ / filename).lexically_normal();
    }

    [[nodiscard]] entt::entity player() const {
        auto player_view = scene_->getRegistry().view<game::component::PlayerTag>();
        EXPECT_NE(player_view.begin(), player_view.end());
        return player_view.begin() == player_view.end() ? entt::null : *player_view.begin();
    }

    game::component::PlayerWalletComponent& wallet() {
        return scene_->getRegistry().get<game::component::PlayerWalletComponent>(player());
    }

    game::component::InventoryComponent& inventory() {
        return scene_->getRegistry().get<game::component::InventoryComponent>(player());
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
            GTEST_SKIP() << "Unable to locate runtime assets directory for shop save roundtrip test.";
        }

        std::error_code ec;
        std::filesystem::current_path(runtime_root_, ec);
        if (ec) {
            GTEST_SKIP() << "Failed to switch working directory to runtime root.";
        }

        temp_dir_ = std::filesystem::temp_directory_path() /
                    ("tinyfarm-shop-save-roundtrip-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(temp_dir_, ec);
        if (ec) {
            GTEST_SKIP() << "Failed to create temp directory for shop save roundtrip test.";
        }

        window_ = SDL_CreateWindow("ShopSaveRoundtripTest", 640, 360, SDL_WINDOW_HIDDEN);
        if (!window_) {
            GTEST_SKIP() << "Failed to create SDL window.";
        }

        game_state_ = engine::core::GameState::create(window_);
        if (!game_state_) {
            GTEST_SKIP() << "Failed to create GameState.";
        }
        game_state_->setWindowSize({640.0F, 360.0F});
        game_state_->setLogicalSize({640.0F, 360.0F});

        input_config_path_ = temp_dir_ / "input_config.json";
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
        ASSERT_NE(context_, nullptr);

        scene_ = std::make_unique<TestScene>(*context_);
        ASSERT_TRUE(scene_->init());

        auto& registry = scene_->getRegistry();
        auto& game_time = registry.ctx().emplace<game::data::GameTime>();
        game_time.day_ = 3;
        game_time.hour_ = 8.0f;
        game_time.minute_ = 30.0f;
        game_time.time_scale_ = 1.0f;
        game_time.paused_ = false;
        game_time.time_of_day_ = game_time.calculateTimeOfDay(game_time.hour_);

        const entt::entity player_entity = registry.create();
        registry.emplace<game::component::PlayerTag>(player_entity);
        registry.emplace<engine::component::TransformComponent>(player_entity, glm::vec2{128.0f, 128.0f});
        registry.emplace<game::component::InventoryComponent>(player_entity);
        registry.emplace<game::component::HotbarComponent>(player_entity);
        registry.emplace<game::component::PlayerWalletComponent>(
            player_entity,
            game::component::PlayerWalletComponent{.gold_ = 123});
        registry.emplace<game::component::QuestLogComponent>(player_entity);
        registry.emplace<game::component::StateComponent>(player_entity);

        const entt::id_type initial_map_id = entt::hashed_string{"home_exterior"}.value();
        ASSERT_TRUE(world_state_.loadFromWorldFile("assets/maps/farm-rpg.world", initial_map_id, "assets/maps/"));

        entity_factory_ = std::make_unique<game::factory::EntityFactory>(
            registry, blueprint_manager_, &context_->getSpatialIndexManager(), &context_->getAutoTileLibrary());
        map_manager_ = std::make_unique<game::world::MapManager>(
            *scene_, *context_, registry, world_state_, *entity_factory_, blueprint_manager_);

        game::world::MapLoadingSettings settings{};
        settings.async_preload_enabled = false;
        map_manager_->setLoadingSettings(settings);
        ASSERT_TRUE(map_manager_->loadMap(initial_map_id));

        save_service_ = std::make_unique<SaveService>(
            *context_, registry, world_state_, *map_manager_, blueprint_manager_);
    }

    void TearDown() override {
        save_service_.reset();
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
        resource_manager_.reset();
        input_manager_.reset();
        game_state_.reset();

        if (window_ != nullptr) {
            SDL_DestroyWindow(window_);
            window_ = nullptr;
        }

        std::error_code ec;
        if (!input_config_path_.empty()) {
            std::filesystem::remove(input_config_path_, ec);
        }
        if (!temp_dir_.empty()) {
            std::filesystem::remove_all(temp_dir_, ec);
        }
        if (!original_working_dir_.empty()) {
            std::filesystem::current_path(original_working_dir_, ec);
        }
    }
};

TEST_F(ShopSaveRoundtripTest, RoundtripRestoresBuyMutationWithoutShopSpecificSaveSchema) {
    auto& registry = scene_->getRegistry();
    auto item_catalog = loadProjectItemCatalog();
    auto shop_catalog = loadProjectShopCatalog();
    game::domain::InventoryDomainService inventory_domain_service(registry, dispatcher_, item_catalog);
    game::domain::ShopTransactionService shop_transaction_service(
        registry,
        item_catalog,
        shop_catalog,
        inventory_domain_service);

    const auto result = shop_transaction_service.commitBuy(player(), "shop.village.general", "potion"_hs, 2);
    ASSERT_TRUE(result.completed());
    ASSERT_EQ(wallet().gold_, 63);
    ASSERT_EQ(inventory().slot(0).item_id_, "potion"_hs);
    ASSERT_EQ(inventory().slot(0).count_, 2);

    std::string save_error;
    ASSERT_TRUE(save_service_->saveToFile(tempFilePath("shop_buy_roundtrip.json"), save_error)) << save_error;

    wallet().gold_ = 999;
    inventory().clearAll();

    std::string load_error;
    ASSERT_TRUE(save_service_->loadFromFile(tempFilePath("shop_buy_roundtrip.json"), load_error)) << load_error;

    EXPECT_EQ(wallet().gold_, 63);
    EXPECT_EQ(inventory().slot(0).item_id_, "potion"_hs);
    EXPECT_EQ(inventory().slot(0).count_, 2);
}

TEST_F(ShopSaveRoundtripTest, RoundtripRestoresSellMutationWithoutShopSpecificSaveSchema) {
    auto& registry = scene_->getRegistry();
    auto item_catalog = loadProjectItemCatalog();
    auto shop_catalog = loadProjectShopCatalog();
    game::domain::InventoryDomainService inventory_domain_service(registry, dispatcher_, item_catalog);
    game::domain::ShopTransactionService shop_transaction_service(
        registry,
        item_catalog,
        shop_catalog,
        inventory_domain_service);

    inventory().slot(0).item_id_ = "potion"_hs;
    inventory().slot(0).count_ = 3;

    const auto result = shop_transaction_service.commitSell(player(), "potion"_hs, 3, 0);
    ASSERT_TRUE(result.completed());
    ASSERT_EQ(wallet().gold_, 168);
    EXPECT_TRUE(inventory().slot(0).empty());

    std::string save_error;
    ASSERT_TRUE(save_service_->saveToFile(tempFilePath("shop_sell_roundtrip.json"), save_error)) << save_error;

    wallet().gold_ = 1;
    inventory().slot(0).item_id_ = "material_timber"_hs;
    inventory().slot(0).count_ = 7;

    std::string load_error;
    ASSERT_TRUE(save_service_->loadFromFile(tempFilePath("shop_sell_roundtrip.json"), load_error)) << load_error;

    EXPECT_EQ(wallet().gold_, 168);
    EXPECT_TRUE(inventory().slot(0).empty());
}

} // namespace
} // namespace game::save
// NOLINTEND
