#include <gtest/gtest.h>

#include <SDL3/SDL.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include <entt/core/hashed_string.hpp>
#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>

#include "engine/component/collider_component.h"
#include "engine/component/tags.h"
#include "engine/component/transform_component.h"
#include "engine/core/game_state.h"
#include "engine/input/input_manager.h"
#include "engine/spatial/spatial_index_manager.h"

#include "game/component/chest_component.h"
#include "game/component/inventory_component.h"
#include "game/component/map_component.h"
#include "game/component/merchant_component.h"
#include "game/component/npc_component.h"
#include "game/component/player_wallet_component.h"
#include "game/component/quest_giver_component.h"
#include "game/component/quest_log_component.h"
#include "game/component/state_component.h"
#include "game/component/tags.h"
#include "game/data/item_catalog.h"
#include "game/data/quest_catalog.h"
#include "game/defs/commands.h"
#include "game/defs/events.h"
#include "game/defs/spatial_layers.h"
#include "game/domain/inventory_domain_service.h"
#include "game/domain/quest_turn_in_service.h"
#include "game/system/dialogue_system.h"
#include "game/system/interaction_system.h"
#include "game/system/quest_interaction_system.h"
#include "game/world/world_state.h"

using namespace entt::literals;

namespace {

constexpr std::string_view QUEST_ID = "quest.test.hunter_trial";

[[nodiscard]] bool initSdlVideoWithDummyFallback(const Uint32 flags) {
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
    return SDL_Init(flags);
}

[[nodiscard]] std::string testQuestConfigPath() {
    return std::string(PROJECT_SOURCE_DIR) + "/tests/data/quest_interaction_quests.json";
}

[[nodiscard]] std::string testItemConfigPath() {
    return std::string(PROJECT_SOURCE_DIR) + "/tests/data/item_use_items.json";
}

[[nodiscard]] game::data::QuestCatalog loadQuestCatalog() {
    game::data::QuestCatalog catalog;
    EXPECT_TRUE(catalog.loadFromFile(testQuestConfigPath()));
    return catalog;
}

[[nodiscard]] game::data::ItemCatalog loadItemCatalog() {
    game::data::ItemCatalog catalog;
    EXPECT_TRUE(catalog.loadItemConfig(testItemConfigPath()));
    return catalog;
}

struct DialogueCapture {
    std::vector<game::defs::DialogueShowEvent> shows{};

    void onShow(const game::defs::DialogueShowEvent& evt) {
        shows.push_back(evt);
    }
};

[[nodiscard]] entt::entity createPlayer(entt::registry& registry) {
    const entt::entity player = registry.create();
    registry.emplace<game::component::PlayerTag>(player);
    registry.emplace<engine::component::TransformComponent>(player, glm::vec2(16.0f, 16.0f));
    registry.emplace<game::component::InventoryComponent>(player);
    registry.emplace<game::component::PlayerWalletComponent>(player);
    registry.emplace<game::component::QuestLogComponent>(player);
    return player;
}

[[nodiscard]] entt::entity createQuestGiver(entt::registry& registry, const std::string_view quest_id = QUEST_ID) {
    const entt::entity giver = registry.create();
    registry.emplace<engine::component::TransformComponent>(giver, glm::vec2(32.0f, 16.0f));
    registry.emplace<game::component::QuestGiverComponent>(
        giver,
        game::component::QuestGiverComponent{
            .quest_id_ = std::string(quest_id),
            .quest_id_hash_ = entt::hashed_string{quest_id.data(), quest_id.size()}.value()});
    return giver;
}

[[nodiscard]] entt::entity createMerchant(entt::registry& registry,
                                          const std::string_view shop_id = "shop.village.general") {
    const entt::entity merchant = registry.create();
    registry.emplace<engine::component::TransformComponent>(merchant, glm::vec2(32.0f, 16.0f));
    registry.emplace<game::component::MerchantComponent>(
        merchant,
        game::component::MerchantComponent{
            .shop_id_ = std::string(shop_id),
            .shop_id_hash_ = entt::hashed_string{shop_id.data(), shop_id.size()}.value()});
    return merchant;
}

class QuestInteractionPriorityTest : public ::testing::Test {
protected:
    SDL_Window* window_{nullptr};
    std::unique_ptr<engine::core::GameState> game_state_;
    std::unique_ptr<entt::dispatcher> dispatcher_;
    std::filesystem::path config_path_;
    static inline bool sdl_ready_{false};

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

        dispatcher_ = std::make_unique<entt::dispatcher>();
        window_ = SDL_CreateWindow("QuestInteractionPriorityTest", 320, 240, SDL_WINDOW_HIDDEN);
        ASSERT_NE(window_, nullptr);

        game_state_ = engine::core::GameState::create(window_);
        ASSERT_NE(game_state_, nullptr);

        const auto timestamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        config_path_ = std::filesystem::temp_directory_path() /
                       ("quest_interaction_priority_test_config_" + std::to_string(timestamp) + ".json");

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
        dispatcher_.reset();

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

    void onEvent(const game::defs::InteractCommand& evt) {
        events.push_back(evt);
    }
};

} // namespace

namespace game::system {

TEST(QuestInteractionSystemTest, OfferableInteractionAcceptsQuestAndInitializesProgressKeys) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    auto item_catalog = loadItemCatalog();
    auto catalog = loadQuestCatalog();
    ASSERT_NE(catalog.findQuest(QUEST_ID), nullptr);
    game::domain::InventoryDomainService inventory_domain_service(registry, dispatcher, item_catalog);
    game::domain::QuestTurnInService turn_in_service(registry, item_catalog, inventory_domain_service);

    QuestInteractionSystem system(registry, dispatcher, catalog, turn_in_service);
    DialogueCapture capture{};
    dispatcher.sink<game::defs::DialogueShowEvent>().connect<&DialogueCapture::onShow>(&capture);

    const entt::entity player = createPlayer(registry);
    const entt::entity giver = createQuestGiver(registry);

    dispatcher.trigger(game::defs::InteractCommand{player, giver});

    auto& quest_log = registry.get<game::component::QuestLogComponent>(player);
    ASSERT_EQ(quest_log.active_quests.size(), 1u);
    EXPECT_EQ(quest_log.active_quests.front(), QUEST_ID);
    EXPECT_TRUE(quest_log.completed_quests.empty());
    EXPECT_EQ(quest_log.objective_progress[game::data::makeQuestObjectiveProgressKey(QUEST_ID, "slime_hunt")], 0);
    EXPECT_EQ(quest_log.objective_progress[game::data::makeQuestObjectiveProgressKey(QUEST_ID, "bat_hunt")], 0);

    ASSERT_EQ(capture.shows.size(), 1u);
    EXPECT_EQ(capture.shows.front().target, giver);
    EXPECT_EQ(capture.shows.front().channel, 1);
    EXPECT_EQ(capture.shows.front().text, "Take this hunt.");
}

TEST(QuestInteractionSystemTest, ActiveQuestShowsProgressTextWithoutDuplicatingQuestEntry) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    auto item_catalog = loadItemCatalog();
    auto catalog = loadQuestCatalog();
    game::domain::InventoryDomainService inventory_domain_service(registry, dispatcher, item_catalog);
    game::domain::QuestTurnInService turn_in_service(registry, item_catalog, inventory_domain_service);

    QuestInteractionSystem system(registry, dispatcher, catalog, turn_in_service);
    DialogueCapture capture{};
    dispatcher.sink<game::defs::DialogueShowEvent>().connect<&DialogueCapture::onShow>(&capture);

    const entt::entity player = createPlayer(registry);
    const entt::entity giver = createQuestGiver(registry);

    auto& quest_log = registry.get<game::component::QuestLogComponent>(player);
    quest_log.active_quests.push_back(std::string(QUEST_ID));
    quest_log.objective_progress[game::data::makeQuestObjectiveProgressKey(QUEST_ID, "slime_hunt")] = 1;
    quest_log.objective_progress[game::data::makeQuestObjectiveProgressKey(QUEST_ID, "bat_hunt")] = 0;

    dispatcher.trigger(game::defs::InteractCommand{player, giver});

    EXPECT_EQ(std::count(quest_log.active_quests.begin(), quest_log.active_quests.end(), QUEST_ID), 1);
    ASSERT_EQ(capture.shows.size(), 1u);
    EXPECT_EQ(capture.shows.front().channel, 1);
    EXPECT_EQ(capture.shows.front().text, "Keep going.");
}

TEST(QuestInteractionSystemTest, ReadyToTurnInCompletesQuestAndShowsRewardSummary) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    auto item_catalog = loadItemCatalog();
    auto catalog = loadQuestCatalog();
    game::domain::InventoryDomainService inventory_domain_service(registry, dispatcher, item_catalog);
    game::domain::QuestTurnInService turn_in_service(registry, item_catalog, inventory_domain_service);

    QuestInteractionSystem system(registry, dispatcher, catalog, turn_in_service);
    DialogueCapture capture{};
    dispatcher.sink<game::defs::DialogueShowEvent>().connect<&DialogueCapture::onShow>(&capture);

    const entt::entity player = createPlayer(registry);
    const entt::entity giver = createQuestGiver(registry);

    auto& quest_log = registry.get<game::component::QuestLogComponent>(player);
    quest_log.active_quests.push_back(std::string(QUEST_ID));
    quest_log.objective_progress[game::data::makeQuestObjectiveProgressKey(QUEST_ID, "slime_hunt")] = 2;
    quest_log.objective_progress[game::data::makeQuestObjectiveProgressKey(QUEST_ID, "bat_hunt")] = 1;

    dispatcher.trigger(game::defs::InteractCommand{player, giver});

    EXPECT_TRUE(quest_log.active_quests.empty());
    ASSERT_EQ(quest_log.completed_quests.size(), 1u);
    EXPECT_EQ(quest_log.completed_quests.front(), QUEST_ID);
    EXPECT_FALSE(quest_log.objective_progress.contains(game::data::makeQuestObjectiveProgressKey(QUEST_ID, "slime_hunt")));
    EXPECT_FALSE(quest_log.objective_progress.contains(game::data::makeQuestObjectiveProgressKey(QUEST_ID, "bat_hunt")));
    const auto& wallet = registry.get<game::component::PlayerWalletComponent>(player);
    EXPECT_EQ(wallet.gold_, 4);
    const auto& inventory = registry.get<game::component::InventoryComponent>(player);
    EXPECT_EQ(inventory.slot(0).item_id_, entt::hashed_string{"strawberry_seed"}.value());
    EXPECT_EQ(inventory.slot(0).count_, 2);
    ASSERT_EQ(capture.shows.size(), 1u);
    EXPECT_EQ(capture.shows.front().text, "Thanks for finishing the hunt.\n获得金币 4\n获得 Strawberry Seed x2");
}

TEST(QuestInteractionSystemTest, ReadyToTurnInFailureKeepsQuestReadyAndShowsFailureMessage) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    auto item_catalog = loadItemCatalog();
    auto catalog = loadQuestCatalog();
    game::domain::InventoryDomainService inventory_domain_service(registry, dispatcher, item_catalog);
    game::domain::QuestTurnInService turn_in_service(registry, item_catalog, inventory_domain_service);

    QuestInteractionSystem system(registry, dispatcher, catalog, turn_in_service);
    DialogueCapture capture{};
    dispatcher.sink<game::defs::DialogueShowEvent>().connect<&DialogueCapture::onShow>(&capture);

    const entt::entity player = createPlayer(registry);
    const entt::entity giver = createQuestGiver(registry);

    auto& inventory = registry.get<game::component::InventoryComponent>(player);
    for (int i = 0; i < inventory.slotCount(); ++i) {
        inventory.slot(i).item_id_ = entt::hashed_string{"dummy_full"}.value();
        inventory.slot(i).count_ = 1;
    }

    auto& quest_log = registry.get<game::component::QuestLogComponent>(player);
    quest_log.active_quests.push_back(std::string(QUEST_ID));
    quest_log.objective_progress[game::data::makeQuestObjectiveProgressKey(QUEST_ID, "slime_hunt")] = 2;
    quest_log.objective_progress[game::data::makeQuestObjectiveProgressKey(QUEST_ID, "bat_hunt")] = 1;

    dispatcher.trigger(game::defs::InteractCommand{player, giver});

    ASSERT_EQ(quest_log.active_quests.size(), 1u);
    EXPECT_EQ(quest_log.active_quests.front(), QUEST_ID);
    EXPECT_TRUE(quest_log.completed_quests.empty());
    EXPECT_EQ(registry.get<game::component::PlayerWalletComponent>(player).gold_, 0);
    ASSERT_EQ(capture.shows.size(), 1u);
    EXPECT_EQ(capture.shows.front().text, "背包空间不足");
}

TEST(QuestInteractionSystemTest, CompletedQuestShowsCompletedText) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    auto item_catalog = loadItemCatalog();
    auto catalog = loadQuestCatalog();
    game::domain::InventoryDomainService inventory_domain_service(registry, dispatcher, item_catalog);
    game::domain::QuestTurnInService turn_in_service(registry, item_catalog, inventory_domain_service);

    QuestInteractionSystem system(registry, dispatcher, catalog, turn_in_service);
    DialogueCapture capture{};
    dispatcher.sink<game::defs::DialogueShowEvent>().connect<&DialogueCapture::onShow>(&capture);

    const entt::entity player = createPlayer(registry);
    const entt::entity giver = createQuestGiver(registry);

    auto& quest_log = registry.get<game::component::QuestLogComponent>(player);
    quest_log.completed_quests.push_back(std::string(QUEST_ID));

    dispatcher.trigger(game::defs::InteractCommand{player, giver});

    ASSERT_EQ(capture.shows.size(), 1u);
    EXPECT_EQ(capture.shows.front().text, "Thanks for finishing the hunt.");
    EXPECT_TRUE(quest_log.active_quests.empty());
}

TEST(QuestInteractionSystemTest, MissingQuestDefinitionDoesNotModifyQuestLogOrEmitFeedback) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    auto item_catalog = loadItemCatalog();
    auto catalog = loadQuestCatalog();
    game::domain::InventoryDomainService inventory_domain_service(registry, dispatcher, item_catalog);
    game::domain::QuestTurnInService turn_in_service(registry, item_catalog, inventory_domain_service);

    QuestInteractionSystem system(registry, dispatcher, catalog, turn_in_service);
    DialogueCapture capture{};
    dispatcher.sink<game::defs::DialogueShowEvent>().connect<&DialogueCapture::onShow>(&capture);

    const entt::entity player = createPlayer(registry);
    const entt::entity giver = createQuestGiver(registry, "quest.test.missing");

    dispatcher.trigger(game::defs::InteractCommand{player, giver});

    const auto& quest_log = registry.get<game::component::QuestLogComponent>(player);
    EXPECT_TRUE(quest_log.active_quests.empty());
    EXPECT_TRUE(quest_log.completed_quests.empty());
    EXPECT_TRUE(quest_log.objective_progress.empty());
    EXPECT_TRUE(capture.shows.empty());
}

TEST(QuestInteractionSystemTest, DialogueSystemSkipsQuestGiverTargets) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    DialogueSystem system(registry, dispatcher);
    ASSERT_TRUE(system.loadDialogueFile(std::string(PROJECT_SOURCE_DIR) + "/assets/data/dialogue_script.json"));

    DialogueCapture capture{};
    dispatcher.sink<game::defs::DialogueShowEvent>().connect<&DialogueCapture::onShow>(&capture);

    const entt::entity player = createPlayer(registry);
    const entt::entity giver = createQuestGiver(registry);
    auto& dialogue = registry.emplace<game::component::DialogueComponent>(
        giver,
        game::component::DialogueComponent{entt::hashed_string{"friend_intro"}.value()});

    dispatcher.trigger(game::defs::InteractCommand{player, giver});

    EXPECT_TRUE(capture.shows.empty());
    EXPECT_FALSE(dialogue.active_);
    EXPECT_EQ(dialogue.current_line_, 0u);
}

TEST(QuestInteractionSystemTest, DialogueSystemSkipsMerchantTargets) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    DialogueSystem system(registry, dispatcher);
    ASSERT_TRUE(system.loadDialogueFile(std::string(PROJECT_SOURCE_DIR) + "/assets/data/dialogue_script.json"));

    DialogueCapture capture{};
    dispatcher.sink<game::defs::DialogueShowEvent>().connect<&DialogueCapture::onShow>(&capture);

    const entt::entity player = createPlayer(registry);
    const entt::entity merchant = createMerchant(registry);
    auto& dialogue = registry.emplace<game::component::DialogueComponent>(
        merchant,
        game::component::DialogueComponent{entt::hashed_string{"friend_intro"}.value()});

    dispatcher.trigger(game::defs::InteractCommand{player, merchant});

    EXPECT_TRUE(capture.shows.empty());
    EXPECT_FALSE(dialogue.active_);
    EXPECT_EQ(dialogue.current_line_, 0u);
}

TEST(QuestInteractionSystemTest, QuestInteractionSystemSkipsMerchantTargets) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    auto item_catalog = loadItemCatalog();
    auto catalog = loadQuestCatalog();
    game::domain::InventoryDomainService inventory_domain_service(registry, dispatcher, item_catalog);
    game::domain::QuestTurnInService turn_in_service(registry, item_catalog, inventory_domain_service);

    QuestInteractionSystem system(registry, dispatcher, catalog, turn_in_service);

    const entt::entity player = createPlayer(registry);
    const entt::entity merchant = createMerchant(registry);
    registry.emplace<game::component::QuestGiverComponent>(
        merchant,
        game::component::QuestGiverComponent{
            .quest_id_ = std::string(QUEST_ID),
            .quest_id_hash_ = entt::hashed_string{QUEST_ID.data(), QUEST_ID.size()}.value()});

    dispatcher.trigger(game::defs::InteractCommand{player, merchant});

    auto& quest_log = registry.get<game::component::QuestLogComponent>(player);
    EXPECT_TRUE(quest_log.active_quests.empty());
    EXPECT_TRUE(quest_log.completed_quests.empty());
}

TEST_F(QuestInteractionPriorityTest, InteractionSystemPrefersQuestGiverOverDialogueChestAndRest) {
    entt::registry registry;
    engine::spatial::SpatialIndexManager spatial;
    constexpr glm::ivec2 MAP_SIZE(4, 4);
    constexpr glm::ivec2 TILE_SIZE(16, 16);
    spatial.initialize(registry,
                       MAP_SIZE,
                       TILE_SIZE,
                       glm::vec2(0.0f, 0.0f),
                       glm::vec2(64.0f, 64.0f),
                       glm::vec2(16.0f, 16.0f));

    game::world::WorldState world_state;
    const entt::id_type map_id = world_state.ensureExternalMap("quest_priority_test_map");
    world_state.setCurrentMap(map_id);
    if (auto* map_state = world_state.getMapStateMutable(map_id)) {
        map_state->info.size_px = glm::ivec2(64, 64);
    }

    auto input = engine::input::InputManager::create(dispatcher_.get(), game_state_.get(), config_path_.string());
    ASSERT_NE(input, nullptr);

    const entt::entity player = registry.create();
    registry.emplace<game::component::PlayerTag>(player);
    registry.emplace<engine::component::TransformComponent>(player, glm::vec2(16.0f, 16.0f));
    auto& state = registry.emplace<game::component::StateComponent>(player);
    state.direction_ = game::component::Direction::Right;
    registry.emplace<game::component::MapId>(player, map_id);

    const entt::entity quest_giver = registry.create();
    registry.emplace<engine::component::TransformComponent>(quest_giver, glm::vec2(32.0f, 16.0f));
    registry.emplace<engine::component::CircleCollider>(quest_giver, 6.0f, glm::vec2(0.0f));
    registry.emplace<engine::component::SpatialIndexTag>(quest_giver);
    registry.emplace<game::component::MapId>(quest_giver, map_id);
    registry.emplace<game::component::QuestGiverComponent>(
        quest_giver,
        game::component::QuestGiverComponent{
            .quest_id_ = std::string(QUEST_ID),
            .quest_id_hash_ = entt::hashed_string{QUEST_ID.data(), QUEST_ID.size()}.value()});
    registry.emplace<game::component::DialogueComponent>(
        quest_giver,
        game::component::DialogueComponent{entt::hashed_string{"friend_intro"}.value()});
    spatial.updateColliderEntity(quest_giver);

    const entt::entity npc = registry.create();
    registry.emplace<engine::component::TransformComponent>(npc, glm::vec2(34.0f, 16.0f));
    registry.emplace<engine::component::CircleCollider>(npc, 6.0f, glm::vec2(0.0f));
    registry.emplace<engine::component::SpatialIndexTag>(npc);
    registry.emplace<game::component::MapId>(npc, map_id);
    registry.emplace<game::component::DialogueComponent>(npc, game::component::DialogueComponent{"friend_intro"_hs});
    spatial.updateColliderEntity(npc);

    const entt::entity chest = registry.create();
    registry.emplace<engine::component::TransformComponent>(chest, glm::vec2(36.0f, 16.0f));
    registry.emplace<engine::component::CircleCollider>(chest, 6.0f, glm::vec2(0.0f));
    registry.emplace<engine::component::SpatialIndexTag>(chest);
    registry.emplace<game::component::MapId>(chest, map_id);
    registry.emplace<game::component::ChestComponent>(chest);
    spatial.updateColliderEntity(chest);

    const entt::entity rest_area = registry.create();
    registry.emplace<game::component::MapId>(rest_area, map_id);
    registry.emplace<game::component::RestArea>(rest_area, engine::utils::Rect{glm::vec2(32.0f, 16.0f), glm::vec2(TILE_SIZE)});
    spatial.addTileEntity(glm::ivec2(2, 1), rest_area, game::defs::spatial_layer::REST);

    InteractCapture capture{};
    dispatcher_->sink<game::defs::InteractCommand>().connect<&InteractCapture::onEvent>(&capture);

    InteractionSystem system(registry, *dispatcher_, *input, spatial, world_state);

    SDL_Event key_down{};
    key_down.type = SDL_EVENT_KEY_DOWN;
    key_down.key.scancode = SDL_SCANCODE_F;
    key_down.key.down = true;
    key_down.key.repeat = false;
    ASSERT_EQ(SDL_PushEvent(&key_down), true);

    input->update();
    system.update();

    ASSERT_EQ(capture.events.size(), 1u);
    EXPECT_EQ(capture.events.front().player, player);
    EXPECT_EQ(capture.events.front().target, quest_giver);
}

TEST_F(QuestInteractionPriorityTest, InteractionSystemPrefersMerchantOverQuestGiverDialogueChestAndRest) {
    entt::registry registry;
    engine::spatial::SpatialIndexManager spatial;
    constexpr glm::ivec2 MAP_SIZE(4, 4);
    constexpr glm::ivec2 TILE_SIZE(16, 16);
    spatial.initialize(registry,
                       MAP_SIZE,
                       TILE_SIZE,
                       glm::vec2(0.0f, 0.0f),
                       glm::vec2(64.0f, 64.0f),
                       glm::vec2(16.0f, 16.0f));

    game::world::WorldState world_state;
    const entt::id_type map_id = world_state.ensureExternalMap("merchant_priority_test_map");
    world_state.setCurrentMap(map_id);
    if (auto* map_state = world_state.getMapStateMutable(map_id)) {
        map_state->info.size_px = glm::ivec2(64, 64);
    }

    auto input = engine::input::InputManager::create(dispatcher_.get(), game_state_.get(), config_path_.string());
    ASSERT_NE(input, nullptr);

    const entt::entity player = registry.create();
    registry.emplace<game::component::PlayerTag>(player);
    registry.emplace<engine::component::TransformComponent>(player, glm::vec2(16.0f, 16.0f));
    auto& state = registry.emplace<game::component::StateComponent>(player);
    state.direction_ = game::component::Direction::Right;
    registry.emplace<game::component::MapId>(player, map_id);

    const entt::entity merchant = registry.create();
    registry.emplace<engine::component::TransformComponent>(merchant, glm::vec2(32.0f, 16.0f));
    registry.emplace<engine::component::CircleCollider>(merchant, 6.0f, glm::vec2(0.0f));
    registry.emplace<engine::component::SpatialIndexTag>(merchant);
    registry.emplace<game::component::MapId>(merchant, map_id);
    registry.emplace<game::component::MerchantComponent>(
        merchant,
        game::component::MerchantComponent{
            .shop_id_ = "shop.village.general",
            .shop_id_hash_ = entt::hashed_string{"shop.village.general"}.value()});
    registry.emplace<game::component::DialogueComponent>(
        merchant,
        game::component::DialogueComponent{entt::hashed_string{"friend_intro"}.value()});
    spatial.updateColliderEntity(merchant);

    const entt::entity quest_giver = registry.create();
    registry.emplace<engine::component::TransformComponent>(quest_giver, glm::vec2(34.0f, 16.0f));
    registry.emplace<engine::component::CircleCollider>(quest_giver, 6.0f, glm::vec2(0.0f));
    registry.emplace<engine::component::SpatialIndexTag>(quest_giver);
    registry.emplace<game::component::MapId>(quest_giver, map_id);
    registry.emplace<game::component::QuestGiverComponent>(
        quest_giver,
        game::component::QuestGiverComponent{
            .quest_id_ = std::string(QUEST_ID),
            .quest_id_hash_ = entt::hashed_string{QUEST_ID.data(), QUEST_ID.size()}.value()});
    spatial.updateColliderEntity(quest_giver);

    const entt::entity npc = registry.create();
    registry.emplace<engine::component::TransformComponent>(npc, glm::vec2(36.0f, 16.0f));
    registry.emplace<engine::component::CircleCollider>(npc, 6.0f, glm::vec2(0.0f));
    registry.emplace<engine::component::SpatialIndexTag>(npc);
    registry.emplace<game::component::MapId>(npc, map_id);
    registry.emplace<game::component::DialogueComponent>(npc, game::component::DialogueComponent{"friend_intro"_hs});
    spatial.updateColliderEntity(npc);

    const entt::entity chest = registry.create();
    registry.emplace<engine::component::TransformComponent>(chest, glm::vec2(38.0f, 16.0f));
    registry.emplace<engine::component::CircleCollider>(chest, 6.0f, glm::vec2(0.0f));
    registry.emplace<engine::component::SpatialIndexTag>(chest);
    registry.emplace<game::component::MapId>(chest, map_id);
    registry.emplace<game::component::ChestComponent>(chest);
    spatial.updateColliderEntity(chest);

    const entt::entity rest_area = registry.create();
    registry.emplace<game::component::MapId>(rest_area, map_id);
    registry.emplace<game::component::RestArea>(rest_area, engine::utils::Rect{glm::vec2(32.0f, 16.0f), glm::vec2(TILE_SIZE)});
    spatial.addTileEntity(glm::ivec2(2, 1), rest_area, game::defs::spatial_layer::REST);

    InteractCapture capture{};
    dispatcher_->sink<game::defs::InteractCommand>().connect<&InteractCapture::onEvent>(&capture);

    InteractionSystem system(registry, *dispatcher_, *input, spatial, world_state);

    SDL_Event key_down{};
    key_down.type = SDL_EVENT_KEY_DOWN;
    key_down.key.scancode = SDL_SCANCODE_F;
    key_down.key.down = true;
    key_down.key.repeat = false;
    ASSERT_EQ(SDL_PushEvent(&key_down), true);

    input->update();
    system.update();

    ASSERT_EQ(capture.events.size(), 1u);
    EXPECT_EQ(capture.events.front().player, player);
    EXPECT_EQ(capture.events.front().target, merchant);
}

} // namespace game::system
