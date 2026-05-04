// NOLINTBEGIN
#include <gtest/gtest.h>

#include <SDL3/SDL.h>
#include <entt/core/hashed_string.hpp>
#include <entt/signal/dispatcher.hpp>
#include <nlohmann/json.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

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
#include "game/component/hotbar_component.h"
#include "game/component/inventory_component.h"
#include "game/component/party_component.h"
#include "game/component/player_wallet_component.h"
#include "game/component/quest_log_component.h"
#include "game/component/state_component.h"
#include "game/component/tags.h"
#include "game/data/game_time.h"
#include "game/data/quest_data.h"
#include "game/defs/events.h"
#include "game/factory/blueprint_manager.h"
#include "game/factory/entity_factory.h"
#include "game/save/save_service.h"
#include "game/world/map_loading_settings.h"
#include "game/world/map_manager.h"
#include "game/world/world_state.h"
#include "engine/component/transform_component.h"

namespace game::save {
namespace {

[[nodiscard]] bool initSdlVideoWithDummyFallback(Uint32 flags) {
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
    return SDL_Init(flags);
}

[[nodiscard]] std::string readTextFile(const std::filesystem::path& path) {
    std::ifstream file(path);
    EXPECT_TRUE(file.is_open()) << path;
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

class TestScene final : public engine::scene::Scene {
public:
    explicit TestScene(engine::core::Context& context)
        : Scene("SaveServiceAsyncTestScene", context) {}
};

class SaveServiceAsyncBehaviorTest : public ::testing::Test {
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

    struct AsyncSaveEventCollector final {
        std::vector<game::defs::AsyncSaveCompletedEvent> events{};

        void onEvent(const game::defs::AsyncSaveCompletedEvent& event) {
            events.push_back(event);
        }
    };

    void pumpMainThreadAndDispatcherOnce() {
        if (main_thread_command_queue_) {
            (void)main_thread_command_queue_->drain();
        }
        dispatcher_.update();
    }

    [[nodiscard]] std::optional<game::defs::AsyncSaveCompletedEvent> waitForAsyncSaveEvent(
        std::chrono::milliseconds timeout = std::chrono::milliseconds(5000)) {
        AsyncSaveEventCollector collector;
        auto sink = dispatcher_.sink<game::defs::AsyncSaveCompletedEvent>();
        sink.connect<&AsyncSaveEventCollector::onEvent>(collector);

        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            pumpMainThreadAndDispatcherOnce();
            if (!collector.events.empty()) {
                sink.disconnect<&AsyncSaveEventCollector::onEvent>(collector);
                return collector.events.back();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        sink.disconnect<&AsyncSaveEventCollector::onEvent>(collector);
        return std::nullopt;
    }

    [[nodiscard]] std::vector<game::defs::AsyncSaveCompletedEvent> collectAsyncSaveEvents(
        std::size_t expected_count,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(5000)) {
        AsyncSaveEventCollector collector;
        auto sink = dispatcher_.sink<game::defs::AsyncSaveCompletedEvent>();
        sink.connect<&AsyncSaveEventCollector::onEvent>(collector);

        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline && collector.events.size() < expected_count) {
            pumpMainThreadAndDispatcherOnce();
            if (collector.events.size() >= expected_count) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        sink.disconnect<&AsyncSaveEventCollector::onEvent>(collector);
        return collector.events;
    }

    void enlargeOpenedChestState(std::size_t count = 20000) {
        const entt::id_type map_id = map_manager_->currentMapId();
        auto* map_state = world_state_.getMapStateMutable(map_id);
        ASSERT_NE(map_state, nullptr);
        map_state->persistent.opened_chests.clear();
        for (std::size_t i = 0; i < count; ++i) {
            map_state->persistent.opened_chests.insert(static_cast<int>(i));
        }
    }

    [[nodiscard]] std::filesystem::path tempFilePath(std::string_view filename) const {
        return (temp_dir_ / filename).lexically_normal();
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
            GTEST_SKIP() << "Unable to locate runtime assets directory for save service async test.";
        }

        std::error_code ec;
        std::filesystem::current_path(runtime_root_, ec);
        if (ec) {
            GTEST_SKIP() << "Failed to switch working directory to runtime root.";
        }

        temp_dir_ = std::filesystem::temp_directory_path() /
                    ("tinyfarm-save-service-async-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(temp_dir_, ec);
        if (ec) {
            GTEST_SKIP() << "Failed to create temp directory for save service async test.";
        }

        window_ = SDL_CreateWindow("SaveServiceAsyncTest", 640, 360, SDL_WINDOW_HIDDEN);
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
        if (!context_) {
            GTEST_SKIP() << "Failed to create Context.";
        }

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

        const entt::entity player = registry.create();
        registry.emplace<game::component::PlayerTag>(player);
        registry.emplace<engine::component::TransformComponent>(player, glm::vec2{128.0f, 128.0f});
        auto& inventory = registry.emplace<game::component::InventoryComponent>(player);
        inventory.slots_[0].item_id_ = entt::hashed_string{"tool.hoe"}.value();
        inventory.slots_[0].count_ = 1;
        auto& hotbar = registry.emplace<game::component::HotbarComponent>(player);
        hotbar.slots_[0].inventory_slot_index_ = 0;
        registry.emplace<game::component::PlayerWalletComponent>(player, game::component::PlayerWalletComponent{.gold_ = 345});
        auto& quest_log = registry.emplace<game::component::QuestLogComponent>(player);
        quest_log.active_quests = {"quest.village.goblin_cleanup"};
        quest_log.completed_quests = {"quest.tutorial.intro"};
        quest_log.objective_progress = {
            {game::data::makeQuestObjectiveProgressKey(
                 "quest.village.goblin_cleanup",
                 "kill_goblins"),
             2}};
        registry.emplace<game::component::PartyComponent>(
            player,
            game::component::PartyComponent{
                .recruited_actor_ids_ = {"actor.player", "actor.lyria"},
                .active_actor_ids_ = {"actor.player", "actor.lyria"}});
        registry.emplace<game::component::StateComponent>(player);

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

TEST(SaveServiceAsyncTest, ExposesAsyncApi) {
    using AsyncStartSignature = bool (SaveService::*)(const std::filesystem::path&, std::string&);
    using SyncSaveSignature = bool (SaveService::*)(const std::filesystem::path&, std::string&);
    using LoadSignature = bool (SaveService::*)(const std::filesystem::path&, std::string&);
    using IsSavingSignature = bool (SaveService::*)() const;

    static_assert(std::is_same_v<decltype(&SaveService::saveToFileAsync), AsyncStartSignature>);
    static_assert(std::is_same_v<decltype(&SaveService::saveToFile), SyncSaveSignature>);
    static_assert(std::is_same_v<decltype(&SaveService::loadFromFile), LoadSignature>);
    static_assert(std::is_same_v<decltype(&SaveService::isSaving), IsSavingSignature>);
}

TEST_F(SaveServiceAsyncBehaviorTest, AsyncSaveSucceedsAndWritesJsonFile) {
    const auto file_path = tempFilePath("async_save_success.json");
    std::string start_error;
    ASSERT_TRUE(save_service_->saveToFileAsync(file_path, start_error)) << start_error;

    const auto result = waitForAsyncSaveEvent();
    ASSERT_TRUE(result.has_value()) << "Timed out waiting for async save result.";
    EXPECT_TRUE(result->success) << result->error;
    EXPECT_EQ(std::filesystem::path(result->file_path).lexically_normal(), file_path.lexically_normal());
    EXPECT_TRUE(result->error.empty());

    ASSERT_TRUE(std::filesystem::exists(file_path));
    ASSERT_GT(std::filesystem::file_size(file_path), 0U);

    std::ifstream in(file_path);
    ASSERT_TRUE(in.is_open());
    nlohmann::json json;
    in >> json;
    EXPECT_EQ(json.at("schema_version").get<std::uint32_t>(), SAVE_SCHEMA_VERSION);
    EXPECT_TRUE(json.contains("player"));
    EXPECT_TRUE(json.contains("maps"));
    EXPECT_EQ(json.at("player").at("gold").get<int>(), 345);
}

TEST_F(SaveServiceAsyncBehaviorTest, SaveToFileWritesPhase4ExtendedStateContainers) {
    const auto file_path = tempFilePath("save_phase4_extended_state.json");
    std::string save_error;
    ASSERT_TRUE(save_service_->saveToFile(file_path, save_error)) << save_error;

    std::ifstream in(file_path);
    ASSERT_TRUE(in.is_open());
    nlohmann::json json;
    in >> json;

    ASSERT_TRUE(json.contains("quest_state"));
    ASSERT_TRUE(json.at("quest_state").is_object());
    const auto& quest_state = json.at("quest_state");
    EXPECT_TRUE(quest_state.contains("active_quests"));
    EXPECT_TRUE(quest_state.at("active_quests").is_array());
    ASSERT_EQ(quest_state.at("active_quests").size(), 1U);
    EXPECT_EQ(quest_state.at("active_quests").at(0).get<std::string>(), "quest.village.goblin_cleanup");
    EXPECT_TRUE(quest_state.contains("completed_quests"));
    EXPECT_TRUE(quest_state.at("completed_quests").is_array());
    ASSERT_EQ(quest_state.at("completed_quests").size(), 1U);
    EXPECT_EQ(quest_state.at("completed_quests").at(0).get<std::string>(), "quest.tutorial.intro");
    EXPECT_TRUE(quest_state.contains("objective_progress"));
    EXPECT_TRUE(quest_state.at("objective_progress").is_object());
    EXPECT_EQ(
        quest_state.at("objective_progress")
            .at("quest.village.goblin_cleanup::kill_goblins")
            .get<int>(),
        2);

    ASSERT_TRUE(json.contains("skill_state"));
    ASSERT_TRUE(json.at("skill_state").is_object());
    const auto& skill_state = json.at("skill_state");
    EXPECT_TRUE(skill_state.contains("learned_skills"));
    EXPECT_TRUE(skill_state.at("learned_skills").is_array());
    EXPECT_TRUE(skill_state.contains("skill_levels"));
    EXPECT_TRUE(skill_state.at("skill_levels").is_object());
    EXPECT_TRUE(skill_state.contains("skill_cooldowns"));
    EXPECT_TRUE(skill_state.at("skill_cooldowns").is_object());

    ASSERT_TRUE(json.contains("combat_state"));
    ASSERT_TRUE(json.at("combat_state").is_object());
    const auto& combat_state = json.at("combat_state");
    EXPECT_TRUE(combat_state.contains("pending_battle"));
    EXPECT_TRUE(combat_state.at("pending_battle").is_boolean());
    EXPECT_TRUE(combat_state.contains("troop_id"));
    EXPECT_TRUE(combat_state.at("troop_id").is_string());
    EXPECT_TRUE(combat_state.contains("actor_ids"));
    EXPECT_TRUE(combat_state.at("actor_ids").is_array());
    EXPECT_TRUE(combat_state.contains("item_stocks"));
    EXPECT_TRUE(combat_state.at("item_stocks").is_object());
    EXPECT_TRUE(combat_state.contains("escape_attempt_count"));
    EXPECT_TRUE(combat_state.at("escape_attempt_count").is_number_unsigned());

    ASSERT_TRUE(json.contains("party_state"));
    ASSERT_TRUE(json.at("party_state").is_object());
    const auto& party_state = json.at("party_state");
    EXPECT_TRUE(party_state.contains("recruited_actor_ids"));
    EXPECT_TRUE(party_state.at("recruited_actor_ids").is_array());
    EXPECT_TRUE(party_state.contains("active_actor_ids"));
    EXPECT_TRUE(party_state.at("active_actor_ids").is_array());
    ASSERT_EQ(party_state.at("active_actor_ids").size(), 2U);
    EXPECT_EQ(party_state.at("active_actor_ids").at(0).get<std::string>(), "actor.player");
    EXPECT_EQ(party_state.at("active_actor_ids").at(1).get<std::string>(), "actor.lyria");
    ASSERT_TRUE(json.contains("player"));
    EXPECT_TRUE(json.at("player").contains("gold"));
    EXPECT_EQ(json.at("player").at("gold").get<int>(), 345);
}

TEST_F(SaveServiceAsyncBehaviorTest, LoadFromFileRestoresPlayerWalletGold) {
    const auto file_path = tempFilePath("save_wallet_restore.json");
    std::string save_error;
    ASSERT_TRUE(save_service_->saveToFile(file_path, save_error)) << save_error;

    auto player_view = scene_->getRegistry().view<game::component::PlayerTag, game::component::PlayerWalletComponent>();
    ASSERT_NE(player_view.begin(), player_view.end());
    const entt::entity player = *player_view.begin();
    auto& wallet = player_view.get<game::component::PlayerWalletComponent>(player);
    wallet.gold_ = 12;

    std::string load_error;
    ASSERT_TRUE(save_service_->loadFromFile(file_path, load_error)) << load_error;

    player_view = scene_->getRegistry().view<game::component::PlayerTag, game::component::PlayerWalletComponent>();
    ASSERT_NE(player_view.begin(), player_view.end());
    const entt::entity loaded_player = *player_view.begin();
    const auto& loaded_wallet = player_view.get<game::component::PlayerWalletComponent>(loaded_player);
    EXPECT_EQ(loaded_wallet.gold_, 345);
}

TEST_F(SaveServiceAsyncBehaviorTest, LoadFromFileRestoresQuestLogState) {
    const auto file_path = tempFilePath("save_quest_log_restore.json");
    std::string save_error;
    ASSERT_TRUE(save_service_->saveToFile(file_path, save_error)) << save_error;

    auto player_view = scene_->getRegistry().view<game::component::PlayerTag, game::component::QuestLogComponent>();
    ASSERT_NE(player_view.begin(), player_view.end());
    const entt::entity player = *player_view.begin();
    auto& quest_log = player_view.get<game::component::QuestLogComponent>(player);
    quest_log.active_quests = {"quest.changed"};
    quest_log.completed_quests.clear();
    quest_log.objective_progress.clear();

    std::string load_error;
    ASSERT_TRUE(save_service_->loadFromFile(file_path, load_error)) << load_error;

    player_view = scene_->getRegistry().view<game::component::PlayerTag, game::component::QuestLogComponent>();
    ASSERT_NE(player_view.begin(), player_view.end());
    const entt::entity loaded_player = *player_view.begin();
    const auto& loaded_quest_log = player_view.get<game::component::QuestLogComponent>(loaded_player);
    EXPECT_EQ(loaded_quest_log.active_quests, std::vector<std::string>({"quest.village.goblin_cleanup"}));
    EXPECT_EQ(loaded_quest_log.completed_quests, std::vector<std::string>({"quest.tutorial.intro"}));
    ASSERT_EQ(loaded_quest_log.objective_progress.size(), 1U);
    EXPECT_EQ(
        loaded_quest_log.objective_progress.at(
            game::data::makeQuestObjectiveProgressKey(
                "quest.village.goblin_cleanup",
                "kill_goblins")),
        2);
}

TEST_F(SaveServiceAsyncBehaviorTest, LoadFromFileRestoresPartyState) {
    const auto file_path = tempFilePath("save_party_restore.json");
    std::string save_error;
    ASSERT_TRUE(save_service_->saveToFile(file_path, save_error)) << save_error;

    auto player_view = scene_->getRegistry().view<game::component::PlayerTag, game::component::PartyComponent>();
    ASSERT_NE(player_view.begin(), player_view.end());
    const entt::entity player = *player_view.begin();
    auto& party = player_view.get<game::component::PartyComponent>(player);
    party.recruited_actor_ids_ = {"actor.player"};
    party.active_actor_ids_ = {"actor.player"};

    std::string load_error;
    ASSERT_TRUE(save_service_->loadFromFile(file_path, load_error)) << load_error;

    player_view = scene_->getRegistry().view<game::component::PlayerTag, game::component::PartyComponent>();
    ASSERT_NE(player_view.begin(), player_view.end());
    const entt::entity loaded_player = *player_view.begin();
    const auto& loaded_party = player_view.get<game::component::PartyComponent>(loaded_player);
    EXPECT_EQ(loaded_party.recruited_actor_ids_, std::vector<std::string>({"actor.player", "actor.lyria"}));
    EXPECT_EQ(loaded_party.active_actor_ids_, std::vector<std::string>({"actor.player", "actor.lyria"}));
}

TEST_F(SaveServiceAsyncBehaviorTest, LoadFromFileRestoresDefeatedEncounters) {
    const entt::id_type map_id = map_manager_->currentMapId();
    auto* map_state = world_state_.getMapStateMutable(map_id);
    ASSERT_NE(map_state, nullptr);
    map_state->persistent.defeated_encounters = {1001, 1002};

    const auto file_path = tempFilePath("save_defeated_encounters_restore.json");
    std::string save_error;
    ASSERT_TRUE(save_service_->saveToFile(file_path, save_error)) << save_error;

    map_state->persistent.defeated_encounters.clear();

    std::string load_error;
    ASSERT_TRUE(save_service_->loadFromFile(file_path, load_error)) << load_error;

    map_state = world_state_.getMapStateMutable(map_id);
    ASSERT_NE(map_state, nullptr);
    EXPECT_TRUE(map_state->persistent.defeated_encounters.contains(1001));
    EXPECT_TRUE(map_state->persistent.defeated_encounters.contains(1002));
    EXPECT_EQ(map_state->persistent.defeated_encounters.size(), 2U);
}

TEST_F(SaveServiceAsyncBehaviorTest, SaveToFileFailsWhenPlayerMissingQuestLogComponent) {
    auto player_view = scene_->getRegistry().view<game::component::PlayerTag>();
    ASSERT_NE(player_view.begin(), player_view.end());
    for (const entt::entity player : player_view) {
        scene_->getRegistry().remove<game::component::QuestLogComponent>(player);
    }

    const auto file_path = tempFilePath("save_missing_quest_log.json");
    std::string save_error;
    EXPECT_FALSE(save_service_->saveToFile(file_path, save_error));
    EXPECT_EQ(save_error, "玩家缺少 QuestLogComponent");
}

TEST_F(SaveServiceAsyncBehaviorTest, AsyncSaveReportsWriteFailureForInvalidPath) {
    const std::filesystem::path invalid_path = "/dev/null/tinyfarm_save_async_fail.json";
    std::string start_error;
    ASSERT_TRUE(save_service_->saveToFileAsync(invalid_path, start_error)) << start_error;

    const auto result = waitForAsyncSaveEvent();
    ASSERT_TRUE(result.has_value()) << "Timed out waiting for async failure result.";
    EXPECT_FALSE(result->success);
    EXPECT_FALSE(result->error.empty());
    EXPECT_EQ(std::filesystem::path(result->file_path).lexically_normal(), invalid_path.lexically_normal());
}

TEST_F(SaveServiceAsyncBehaviorTest, RejectsReentrantAsyncSaveRequestsWhileSaving) {
    enlargeOpenedChestState();

    const auto first_path = tempFilePath("async_save_first.json");
    const auto second_path = tempFilePath("async_save_second.json");
    std::string first_error;
    ASSERT_TRUE(save_service_->saveToFileAsync(first_path, first_error)) << first_error;
    EXPECT_TRUE(save_service_->isSaving());

    std::string second_error;
    EXPECT_FALSE(save_service_->saveToFileAsync(second_path, second_error));
    EXPECT_FALSE(second_error.empty());

    const auto result = waitForAsyncSaveEvent(std::chrono::milliseconds(10000));
    ASSERT_TRUE(result.has_value()) << "Timed out waiting for first async save result.";
    EXPECT_TRUE(result->success) << result->error;
    EXPECT_EQ(std::filesystem::path(result->file_path).lexically_normal(), first_path.lexically_normal());
}

TEST_F(SaveServiceAsyncBehaviorTest, DestructorWaitsForInFlightAsyncSave) {
    enlargeOpenedChestState();
    const auto file_path = tempFilePath("async_save_destructor_wait.json");

    {
        SaveService local_save_service(
            *context_,
            scene_->getRegistry(),
            world_state_,
            *map_manager_,
            blueprint_manager_);
        std::string start_error;
        ASSERT_TRUE(local_save_service.saveToFileAsync(file_path, start_error)) << start_error;
    }

    ASSERT_TRUE(std::filesystem::exists(file_path));
    ASSERT_GT(std::filesystem::file_size(file_path), 0U);
}

TEST_F(SaveServiceAsyncBehaviorTest, AsyncSavePublishesCompletionEventForEachRequest) {
    const auto first_path = tempFilePath("async_save_stale_result_1.json");
    const auto second_path = tempFilePath("async_save_stale_result_2.json");

    std::string first_error;
    ASSERT_TRUE(save_service_->saveToFileAsync(first_path, first_error)) << first_error;

    const auto first_deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(5000);
    while (save_service_->isSaving() && std::chrono::steady_clock::now() < first_deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ASSERT_FALSE(save_service_->isSaving()) << "First async save did not complete in time.";

    std::string second_error;
    ASSERT_TRUE(save_service_->saveToFileAsync(second_path, second_error)) << second_error;

    const auto events = collectAsyncSaveEvents(2, std::chrono::milliseconds(10000));
    ASSERT_GE(events.size(), 2U) << "Timed out waiting for async save completion events.";

    bool found_first = false;
    bool found_second = false;
    for (const auto& event : events) {
        if (std::filesystem::path(event.file_path).lexically_normal() == first_path.lexically_normal()) {
            found_first = true;
            EXPECT_TRUE(event.success) << event.error;
        }
        if (std::filesystem::path(event.file_path).lexically_normal() == second_path.lexically_normal()) {
            found_second = true;
            EXPECT_TRUE(event.success) << event.error;
        }
    }

    EXPECT_TRUE(found_first) << "First async save completion event should be delivered.";
    EXPECT_TRUE(found_second) << "Second async save completion event should be delivered.";
}

TEST(SaveServiceAsyncTest, SaveToFileReusesExtractedWriteHelper) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/save/save_service.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = readTextFile(source_path);
    ASSERT_FALSE(source.empty());

    EXPECT_NE(source.find("bool SaveService::writeSaveFile"), std::string::npos)
        << "SaveService should extract JSON dump + tmp write + rename into writeSaveFile.";
    EXPECT_NE(source.find("return writeSaveFile(data, file_path, out_error);"), std::string::npos)
        << "saveToFile should reuse writeSaveFile to keep sync/async behavior consistent.";
}

} // namespace
} // namespace game::save
// NOLINTEND
