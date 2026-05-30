#include <gtest/gtest.h>

#include "game/component/inventory_component.h"
#include "game/component/player_wallet_component.h"
#include "game/component/quest_log_component.h"
#include "game/data/item_catalog.h"
#include "game/data/quest_catalog.h"
#include "game/data/quest_data.h"
#include "game/domain/inventory_domain_service.h"
#include "game/domain/quest_turn_in_service.h"

#include <entt/core/hashed_string.hpp>
#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>

#include <string>
#include <string_view>

namespace {

constexpr std::string_view NO_REWARD_QUEST_ID = "quest.test.turn_in_no_reward";
constexpr std::string_view REWARDED_QUEST_ID = "quest.test.turn_in_rewarded";
constexpr std::string_view CUMULATIVE_QUEST_ID = "quest.test.turn_in_cumulative";

[[nodiscard]] std::string testQuestConfigPath() {
    return std::string(PROJECT_SOURCE_DIR) + "/tests/data/quest_turn_in_quests.json";
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

[[nodiscard]] entt::entity createPlayer(entt::registry& registry) {
    const entt::entity player = registry.create();
    registry.emplace<game::component::QuestLogComponent>(player);
    registry.emplace<game::component::InventoryComponent>(player);
    registry.emplace<game::component::PlayerWalletComponent>(player);
    return player;
}

void markQuestReady(game::component::QuestLogComponent& quest_log, const game::data::QuestData& quest) {
    quest_log.active_quests.push_back(quest.id_);
    for (const auto& objective : quest.objectives_) {
        quest_log.objective_progress[game::data::makeQuestObjectiveProgressKey(quest.id_, objective.id_)] =
            objective.required_count_;
    }
}

void fillInventory(game::component::InventoryComponent& inventory, entt::id_type item_id, int count) {
    for (int i = 0; i < inventory.slotCount(); ++i) {
        inventory.slot(i).item_id_ = item_id;
        inventory.slot(i).count_ = count;
    }
}

[[nodiscard]] game::data::QuestData makeQuestWithMissingRewardItem() {
    game::data::QuestData quest{};
    quest.id_ = "quest.test.turn_in_missing_reward_item";
    quest.id_hash_ = entt::hashed_string{quest.id_.c_str()}.value();
    quest.objectives_.push_back(game::data::QuestObjectiveData{
        .id_ = "slime_hunt",
        .kind_ = game::data::QuestObjectiveKind::DefeatEnemyCount,
        .enemy_id_ = "enemy.slime",
        .enemy_id_hash_ = entt::hashed_string{"enemy.slime"}.value(),
        .required_count_ = 1});
    quest.rewards_.gold_ = 7;
    quest.rewards_.items_.push_back(game::data::QuestRewardItemData{
        .item_id_ = "missing_reward_item",
        .item_id_hash_ = entt::hashed_string{"missing_reward_item"}.value(),
        .count_ = 1});
    return quest;
}

} // namespace

namespace game::domain {

TEST(QuestTurnInServiceTest, CompletesQuestWithoutRewardsAndClearsOnlyMatchingProgressKeys) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    auto item_catalog = loadItemCatalog();
    auto quest_catalog = loadQuestCatalog();
    InventoryDomainService inventory_domain_service(registry, dispatcher, item_catalog);
    QuestTurnInService service(registry, item_catalog, inventory_domain_service);

    const auto* quest = quest_catalog.findQuest(NO_REWARD_QUEST_ID);
    ASSERT_NE(quest, nullptr);

    const entt::entity player = createPlayer(registry);
    auto& quest_log = registry.get<game::component::QuestLogComponent>(player);
    markQuestReady(quest_log, *quest);
    quest_log.objective_progress["quest.other::progress"] = 42;

    const auto result = service.turnIn(player, *quest, quest_log);

    ASSERT_TRUE(result.completed());
    EXPECT_EQ(result.status, QuestTurnInStatus::Completed);
    EXPECT_EQ(result.gold_reward, 0);
    EXPECT_TRUE(result.item_rewards.empty());
    EXPECT_TRUE(quest_log.active_quests.empty());
    ASSERT_EQ(quest_log.completed_quests.size(), 1u);
    EXPECT_EQ(quest_log.completed_quests.front(), NO_REWARD_QUEST_ID);
    EXPECT_FALSE(quest_log.objective_progress.contains(game::data::makeQuestObjectiveProgressKey(NO_REWARD_QUEST_ID, "slime_hunt")));
    EXPECT_EQ(quest_log.objective_progress["quest.other::progress"], 42);
}

TEST(QuestTurnInServiceTest, WritesGoldAndItemsWhenRewardsFit) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    auto item_catalog = loadItemCatalog();
    auto quest_catalog = loadQuestCatalog();
    InventoryDomainService inventory_domain_service(registry, dispatcher, item_catalog);
    QuestTurnInService service(registry, item_catalog, inventory_domain_service);

    const auto* quest = quest_catalog.findQuest(REWARDED_QUEST_ID);
    ASSERT_NE(quest, nullptr);

    const entt::entity player = createPlayer(registry);
    auto& wallet = registry.get<game::component::PlayerWalletComponent>(player);
    wallet.gold_ = 5;

    auto& quest_log = registry.get<game::component::QuestLogComponent>(player);
    markQuestReady(quest_log, *quest);

    const auto result = service.turnIn(player, *quest, quest_log);

    ASSERT_TRUE(result.completed());
    EXPECT_EQ(wallet.gold_, 12);
    ASSERT_EQ(result.item_rewards.size(), 1u);
    EXPECT_EQ(result.item_rewards[0].item_id, "strawberry_seed");
    EXPECT_EQ(result.item_rewards[0].item_name, "Strawberry Seed");
    EXPECT_EQ(result.item_rewards[0].count, 3);

    const auto& inventory = registry.get<game::component::InventoryComponent>(player);
    EXPECT_EQ(inventory.slot(0).item_id_, entt::hashed_string{"strawberry_seed"}.value());
    EXPECT_EQ(inventory.slot(0).count_, 3);
    EXPECT_TRUE(quest_log.active_quests.empty());
    ASSERT_EQ(quest_log.completed_quests.size(), 1u);
    EXPECT_EQ(quest_log.completed_quests.front(), REWARDED_QUEST_ID);
}

TEST(QuestTurnInServiceTest, FullInventoryBlocksTurnInWithoutMutatingQuestOrRewards) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    auto item_catalog = loadItemCatalog();
    auto quest_catalog = loadQuestCatalog();
    InventoryDomainService inventory_domain_service(registry, dispatcher, item_catalog);
    QuestTurnInService service(registry, item_catalog, inventory_domain_service);

    const auto* quest = quest_catalog.findQuest(REWARDED_QUEST_ID);
    ASSERT_NE(quest, nullptr);

    const entt::entity player = createPlayer(registry);
    auto& wallet = registry.get<game::component::PlayerWalletComponent>(player);
    wallet.gold_ = 10;
    auto& inventory = registry.get<game::component::InventoryComponent>(player);
    fillInventory(inventory, entt::hashed_string{"dummy_full"}.value(), 1);

    auto& quest_log = registry.get<game::component::QuestLogComponent>(player);
    markQuestReady(quest_log, *quest);

    const auto result = service.turnIn(player, *quest, quest_log);

    EXPECT_FALSE(result.completed());
    EXPECT_EQ(result.status, QuestTurnInStatus::InventoryFull);
    EXPECT_EQ(result.failure_message, "Inventory full");
    EXPECT_EQ(wallet.gold_, 10);
    EXPECT_EQ(inventory.slot(0).item_id_, entt::hashed_string{"dummy_full"}.value());
    EXPECT_EQ(inventory.slot(0).count_, 1);
    ASSERT_EQ(quest_log.active_quests.size(), 1u);
    EXPECT_EQ(quest_log.active_quests.front(), REWARDED_QUEST_ID);
    EXPECT_TRUE(quest_log.completed_quests.empty());
    EXPECT_TRUE(quest_log.objective_progress.contains(game::data::makeQuestObjectiveProgressKey(REWARDED_QUEST_ID, "slime_hunt")));
}

TEST(QuestTurnInServiceTest, MissingWalletFailsBeforeMutatingQuestState) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    auto item_catalog = loadItemCatalog();
    auto quest_catalog = loadQuestCatalog();
    InventoryDomainService inventory_domain_service(registry, dispatcher, item_catalog);
    QuestTurnInService service(registry, item_catalog, inventory_domain_service);

    const auto* quest = quest_catalog.findQuest(REWARDED_QUEST_ID);
    ASSERT_NE(quest, nullptr);

    const entt::entity player = registry.create();
    registry.emplace<game::component::QuestLogComponent>(player);
    registry.emplace<game::component::InventoryComponent>(player);
    auto& quest_log = registry.get<game::component::QuestLogComponent>(player);
    markQuestReady(quest_log, *quest);

    const auto result = service.turnIn(player, *quest, quest_log);

    EXPECT_FALSE(result.completed());
    EXPECT_EQ(result.status, QuestTurnInStatus::MissingWallet);
    EXPECT_EQ(result.failure_message, "Missing wallet component");
    ASSERT_EQ(quest_log.active_quests.size(), 1u);
    EXPECT_TRUE(quest_log.completed_quests.empty());
}

TEST(QuestTurnInServiceTest, MissingInventoryFailsBeforeMutatingQuestState) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    auto item_catalog = loadItemCatalog();
    auto quest_catalog = loadQuestCatalog();
    InventoryDomainService inventory_domain_service(registry, dispatcher, item_catalog);
    QuestTurnInService service(registry, item_catalog, inventory_domain_service);

    const auto* quest = quest_catalog.findQuest(REWARDED_QUEST_ID);
    ASSERT_NE(quest, nullptr);

    const entt::entity player = registry.create();
    registry.emplace<game::component::QuestLogComponent>(player);
    registry.emplace<game::component::PlayerWalletComponent>(player);
    auto& quest_log = registry.get<game::component::QuestLogComponent>(player);
    markQuestReady(quest_log, *quest);

    const auto result = service.turnIn(player, *quest, quest_log);

    EXPECT_FALSE(result.completed());
    EXPECT_EQ(result.status, QuestTurnInStatus::MissingInventory);
    EXPECT_EQ(result.failure_message, "Missing inventory component");
    ASSERT_EQ(quest_log.active_quests.size(), 1u);
    EXPECT_TRUE(quest_log.completed_quests.empty());
}

TEST(QuestTurnInServiceTest, RewardPreflightUsesSingleAccumulatedInventoryCopy) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    auto item_catalog = loadItemCatalog();
    auto quest_catalog = loadQuestCatalog();
    InventoryDomainService inventory_domain_service(registry, dispatcher, item_catalog);
    QuestTurnInService service(registry, item_catalog, inventory_domain_service);

    const auto* quest = quest_catalog.findQuest(CUMULATIVE_QUEST_ID);
    ASSERT_NE(quest, nullptr);

    const entt::entity player = createPlayer(registry);
    auto& inventory = registry.get<game::component::InventoryComponent>(player);
    fillInventory(inventory, entt::hashed_string{"dummy_full"}.value(), 1);
    inventory.slot(0).clear();

    auto& quest_log = registry.get<game::component::QuestLogComponent>(player);
    markQuestReady(quest_log, *quest);

    const auto result = service.turnIn(player, *quest, quest_log);

    EXPECT_FALSE(result.completed());
    EXPECT_EQ(result.status, QuestTurnInStatus::InventoryFull);
    EXPECT_TRUE(quest_log.completed_quests.empty());
    ASSERT_EQ(quest_log.active_quests.size(), 1u);
    EXPECT_EQ(inventory.slot(0).item_id_, entt::id_type{entt::null});
    EXPECT_EQ(inventory.slot(1).item_id_, entt::hashed_string{"dummy_full"}.value());
}

TEST(QuestTurnInServiceTest, RewardWriteFailureKeepsQuestWalletAndInventoryUnchanged) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    auto item_catalog = loadItemCatalog();
    InventoryDomainService inventory_domain_service(registry, dispatcher, item_catalog);
    QuestTurnInService service(registry, item_catalog, inventory_domain_service);

    const game::data::QuestData quest = makeQuestWithMissingRewardItem();
    const entt::entity player = createPlayer(registry);
    auto& wallet = registry.get<game::component::PlayerWalletComponent>(player);
    wallet.gold_ = 10;
    auto& quest_log = registry.get<game::component::QuestLogComponent>(player);
    markQuestReady(quest_log, quest);
    auto& inventory = registry.get<game::component::InventoryComponent>(player);

    const auto result = service.turnIn(player, quest, quest_log);

    EXPECT_FALSE(result.completed());
    EXPECT_EQ(wallet.gold_, 10);
    EXPECT_TRUE(inventory.slot(0).empty());
    ASSERT_EQ(quest_log.active_quests.size(), 1u);
    EXPECT_EQ(quest_log.active_quests.front(), quest.id_);
    EXPECT_TRUE(quest_log.completed_quests.empty());
    EXPECT_TRUE(quest_log.objective_progress.contains(game::data::makeQuestObjectiveProgressKey(quest.id_, "slime_hunt")));
}

} // namespace game::domain
