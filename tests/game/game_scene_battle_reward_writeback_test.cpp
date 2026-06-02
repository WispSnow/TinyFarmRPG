// NOLINTBEGIN
#include <gtest/gtest.h>

#include "appearance_test_fixture_utils.h"
#include "engine/component/transform_component.h"
#include "game/component/inventory_component.h"
#include "game/component/party_runtime_stats_component.h"
#include "game/component/player_identity_component.h"
#include "game/component/player_wallet_component.h"
#include "game/component/quest_log_component.h"
#include "game/component/tags.h"
#include "game/data/item_catalog.h"
#include "game/data/quest_catalog.h"
#include "game/data/rpg_catalog.h"
#include "game/defs/events.h"
#include "game/domain/actor_progression_service.h"
#include "game/domain/inventory_domain_service.h"
#include "game/runtime/system_bundle.h"
#include "game/scene/game_scene_battle_settlement.h"

#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>

#include <filesystem>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace game::scene {
namespace {

[[nodiscard]] game::battle::BattleUnit makeUnit(const game::battle::BattleUnitId id,
                                                std::string_view name,
                                                const game::battle::BattleSide side,
                                                const int hp,
                                                const int max_hp,
                                                std::optional<std::string> source_enemy_id = std::nullopt,
                                                std::optional<std::string> source_actor_id = std::nullopt) {
    return game::battle::BattleUnit{
        .id = id,
        .name = std::string{name},
        .side = side,
        .hp = hp,
        .max_hp = max_hp,
        .mp = 0,
        .max_mp = 0,
        .attack = 10,
        .defense = 10,
        .magic_attack = 10,
        .magic_defense = 10,
        .speed = 10,
        .luck = 10,
        .source_actor_id = std::move(source_actor_id),
        .source_enemy_id = std::move(source_enemy_id)};
}

struct DialogueCapture final {
    std::vector<game::defs::DialogueShowEvent> shows{};
    std::vector<game::defs::DialogueMoveEvent> moves{};
    std::vector<game::defs::DialogueHideEvent> hides{};

    void onShow(const game::defs::DialogueShowEvent& evt) {
        shows.push_back(evt);
    }

    void onMove(const game::defs::DialogueMoveEvent& evt) {
        moves.push_back(evt);
    }

    void onHide(const game::defs::DialogueHideEvent& evt) {
        hides.push_back(evt);
    }
};

struct PartyRuntimeStatsCapture final {
    std::vector<game::defs::PartyRuntimeStatsChanged> changes{};

    void onChanged(const game::defs::PartyRuntimeStatsChanged& evt) {
        changes.push_back(evt);
    }
};

class GameSceneBattleRewardWritebackTest : public ::testing::Test {
protected:
    entt::registry registry_{};
    entt::dispatcher dispatcher_{};
    game::runtime::GameRuntimeServices services_{};
    std::unordered_map<entt::id_type, int> active_battle_initial_item_stocks_{};
    bool has_active_battle_item_stocks_{false};
    std::shared_ptr<game::data::ItemCatalog> item_catalog_{};
    std::shared_ptr<game::data::QuestCatalog> quest_catalog_{};
    std::shared_ptr<game::data::RpgCatalog> rpg_catalog_{};

    [[nodiscard]] std::filesystem::path writeConfig(std::string_view file_name, std::string_view body) const {
        const auto temp_root = game::test::createUniqueTempDir("game_scene_battle_reward_writeback");
        const auto config_path = temp_root / file_name;
        game::test::writeTextFile(config_path, body);
        return config_path;
    }

    void loadCatalogs() {
        item_catalog_ = std::make_shared<game::data::ItemCatalog>();
        rpg_catalog_ = std::make_shared<game::data::RpgCatalog>();

        const auto items_path = writeConfig(
            "items.json",
            R"json({
  "items": [
    {
      "id": "item.herb",
      "display_name": "Herb",
      "category": "material",
      "stack_limit": 99
    },
    {
      "id": "item.stone",
      "display_name": "Stone",
      "category": "material",
      "stack_limit": 1
    }
  ]
})json");
        ASSERT_TRUE(item_catalog_->loadItemConfig(items_path.string()));

        const auto classes_path = writeConfig(
            "classes.json",
            R"json({
  "classes": [
    {
      "id": "class.hero",
      "display_name": "Hero",
      "exp_curve": { "basis": 30, "extra": 20, "acc_a": 30, "acc_b": 30 },
      "base_params": { "mhp": 100, "mmp": 10, "atk": 12, "def": 10, "mat": 8, "mdf": 8, "agi": 10, "luk": 10 },
      "param_curves": {
        "mhp": { "level_1": 100, "level_99": 198, "shape": "linear" },
        "mmp": { "level_1": 10, "level_99": 108, "shape": "linear" }
      }
    }
  ]
})json");
        ASSERT_TRUE(rpg_catalog_->loadClasses(classes_path.string()));

        const auto actors_path = writeConfig(
            "actors.json",
            R"json({
  "actors": [
    {
      "id": "actor.player",
      "display_name": "Alex",
      "class_id": "class.hero",
      "initial_level": 1,
      "max_level": 99
    }
  ]
})json");
        ASSERT_TRUE(rpg_catalog_->loadActors(actors_path.string()));

        const auto enemies_path = writeConfig(
            "enemies.json",
            R"json({
  "enemies": [
    {
      "id": "enemy.slime",
      "display_name": "Slime",
      "params": { "mhp": 20, "mmp": 0, "atk": 6, "def": 4, "mat": 1, "mdf": 1, "agi": 5, "luk": 3 },
      "exp": 3,
      "gold": 4,
      "drops": [
        { "item_id": "item.herb", "chance": 1.0 }
      ]
    }
  ]
})json");
        ASSERT_TRUE(rpg_catalog_->loadEnemies(enemies_path.string()));

        quest_catalog_ = std::make_shared<game::data::QuestCatalog>();
        const auto quests_path = writeConfig(
            "quests.json",
            R"json({
  "schema_version": 1,
  "quests": [
    {
      "id": "quest.slime_hunt",
      "title": "Slime Hunt",
      "objectives": [
        {
          "id": "slime_count",
          "kind": "defeat_enemy_count",
          "enemy_id": "enemy.slime",
          "required_count": 2
        }
      ]
    }
  ]
})json");
        ASSERT_TRUE(quest_catalog_->loadFromFile(quests_path.string()));

        services_.item_catalog = item_catalog_;
        services_.quest_catalog = quest_catalog_;
        services_.rpg_catalog = rpg_catalog_;
        services_.inventory_domain_service =
            std::make_unique<game::domain::InventoryDomainService>(registry_, dispatcher_, *item_catalog_);
    }

    [[nodiscard]] entt::entity createPlayer(const int initial_gold,
                                            const int herb_count,
                                            const bool fill_inventory = false) {
        const entt::entity player = registry_.create();
        registry_.emplace<game::component::PlayerTag>(player);
        registry_.emplace<engine::component::TransformComponent>(player, glm::vec2{128.0f, 64.0f});
        auto& inventory = registry_.emplace<game::component::InventoryComponent>(player);
        if (herb_count > 0) {
            inventory.slots_[0].item_id_ = game::data::RpgCatalog::hashId("item.herb");
            inventory.slots_[0].count_ = herb_count;
        }
        if (fill_inventory) {
            for (std::size_t index = 1; index < inventory.slots_.size(); ++index) {
                inventory.slots_[index].item_id_ = game::data::RpgCatalog::hashId("item.stone");
                inventory.slots_[index].count_ = 1;
            }
        }
        registry_.emplace<game::component::PlayerWalletComponent>(
            player,
            game::component::PlayerWalletComponent{.gold_ = initial_gold});
        registry_.emplace<game::component::QuestLogComponent>(player);
        return player;
    }

    [[nodiscard]] int countItem(const entt::entity player, const entt::id_type item_id) const {
        const auto& inventory = registry_.get<game::component::InventoryComponent>(player);
        int total = 0;
        for (const auto& slot : inventory.slots_) {
            if (slot.item_id_ == item_id) {
                total += slot.count_;
            }
        }
        return total;
    }

    [[nodiscard]] game::battle::BattleRewardSummary slimeRewardSummary() const {
        return game::battle::BattleRewardSummary{
            .gold_total = 4,
            .exp_total = 0,
            .item_drops = {
                game::battle::BattleRewardItemDrop{
                    .item_id = "item.herb",
                    .item_id_hash = game::data::RpgCatalog::hashId("item.herb"),
                    .count = 1
                }
            }
        };
    }

    void SetUp() override {
        loadCatalogs();
    }
};

TEST_F(GameSceneBattleRewardWritebackTest, VictoryWritesExperienceAndLevelUpResult) {
    DialogueCapture capture;
    auto show_sink = dispatcher_.sink<game::defs::DialogueShowEvent>();
    show_sink.connect<&DialogueCapture::onShow>(&capture);

    const entt::entity player = createPlayer(10, 0);
    auto& runtime_stats = registry_.emplace<game::component::PartyRuntimeStatsComponent>(player);
    runtime_stats.states_by_actor_id_["actor.player"] = game::component::ActorRuntimeState{
        .current_hp = 50,
        .current_mp = 5,
        .level = 1,
        .total_exp = 0,
    };

    const auto* actor = rpg_catalog_->findActor("actor.player");
    ASSERT_NE(actor, nullptr);
    const int level_two_exp =
        game::domain::ActorProgressionService::expForLevel(*rpg_catalog_, *actor, 2);

    auto reward_summary = slimeRewardSummary();
    reward_summary.exp_total = level_two_exp;

    game::defs::BattleEndedEvent evt{};
    evt.outcome = game::battle::BattleOutcome::Victory;
    evt.final_units = {
        makeUnit(1, "Alex", game::battle::BattleSide::Player, 30, 30, std::nullopt, std::string{"actor.player"}),
        makeUnit(101, "Slime", game::battle::BattleSide::Enemy, 0, 20, std::string{"enemy.slime"})};
    evt.reward_summary = reward_summary;

    game::system::helpers::NotificationTimer notification_state{};
    processBattleEndedForGameScene(
        registry_,
        dispatcher_,
        &services_,
        active_battle_initial_item_stocks_,
        has_active_battle_item_stocks_,
        notification_state,
        evt);

    const auto& stored = runtime_stats.states_by_actor_id_.at("actor.player");
    EXPECT_EQ(stored.level, 2);
    EXPECT_EQ(stored.total_exp, level_two_exp);
    EXPECT_EQ(stored.current_hp, 51);
    EXPECT_EQ(stored.current_mp, 6);

    ASSERT_EQ(capture.shows.size(), 1U);
    EXPECT_EQ(
        capture.shows[0].text,
        "Gained Gold 4\nGained EXP " + std::to_string(level_two_exp) + "\nAlex Lv.2 HP +1 MP +1\nGained Herb x1");

    show_sink.disconnect<&DialogueCapture::onShow>(&capture);
}

TEST_F(GameSceneBattleRewardWritebackTest, VictoryLevelUpNotificationUsesCustomPlayerName) {
    DialogueCapture capture;
    auto show_sink = dispatcher_.sink<game::defs::DialogueShowEvent>();
    show_sink.connect<&DialogueCapture::onShow>(&capture);

    const entt::entity player = createPlayer(10, 0);
    registry_.emplace<game::component::PlayerIdentityComponent>(
        player,
        game::component::PlayerIdentityComponent{.display_name_ = "Mina"});
    auto& runtime_stats = registry_.emplace<game::component::PartyRuntimeStatsComponent>(player);
    runtime_stats.states_by_actor_id_["actor.player"] = game::component::ActorRuntimeState{
        .current_hp = 50,
        .current_mp = 5,
        .level = 1,
        .total_exp = 0,
    };

    const auto* actor = rpg_catalog_->findActor("actor.player");
    ASSERT_NE(actor, nullptr);
    const int level_two_exp =
        game::domain::ActorProgressionService::expForLevel(*rpg_catalog_, *actor, 2);

    auto reward_summary = slimeRewardSummary();
    reward_summary.exp_total = level_two_exp;

    game::defs::BattleEndedEvent evt{};
    evt.outcome = game::battle::BattleOutcome::Victory;
    evt.final_units = {
        makeUnit(1, "Alex", game::battle::BattleSide::Player, 30, 30, std::nullopt, std::string{"actor.player"}),
        makeUnit(101, "Slime", game::battle::BattleSide::Enemy, 0, 20, std::string{"enemy.slime"})};
    evt.reward_summary = reward_summary;

    game::system::helpers::NotificationTimer notification_state{};
    processBattleEndedForGameScene(
        registry_,
        dispatcher_,
        &services_,
        active_battle_initial_item_stocks_,
        has_active_battle_item_stocks_,
        notification_state,
        evt);

    ASSERT_EQ(capture.shows.size(), 1U);
    EXPECT_NE(capture.shows[0].text.find("Mina Lv.2"), std::string::npos);
    EXPECT_EQ(capture.shows[0].text.find("Alex Lv.2"), std::string::npos);

    show_sink.disconnect<&DialogueCapture::onShow>(&capture);
}

TEST_F(GameSceneBattleRewardWritebackTest, MaxLevelExperienceDoesNotEmitRuntimeStatsChanged) {
    PartyRuntimeStatsCapture stats_capture;
    auto stats_sink = dispatcher_.sink<game::defs::PartyRuntimeStatsChanged>();
    stats_sink.connect<&PartyRuntimeStatsCapture::onChanged>(&stats_capture);

    const entt::entity player = createPlayer(10, 0);
    auto& runtime_stats = registry_.emplace<game::component::PartyRuntimeStatsComponent>(player);
    const auto* actor = rpg_catalog_->findActor("actor.player");
    ASSERT_NE(actor, nullptr);
    const int max_exp =
        game::domain::ActorProgressionService::expForLevel(*rpg_catalog_, *actor, actor->max_level_);
    runtime_stats.states_by_actor_id_["actor.player"] = game::component::ActorRuntimeState{
        .current_hp = 1,
        .current_mp = 0,
        .level = actor->max_level_,
        .total_exp = max_exp,
    };

    auto reward_summary = slimeRewardSummary();
    reward_summary.exp_total = 25;

    game::defs::BattleEndedEvent evt{};
    evt.outcome = game::battle::BattleOutcome::Victory;
    evt.final_units = {
        makeUnit(1, "Alex", game::battle::BattleSide::Player, 1, 198, std::nullopt, std::string{"actor.player"}),
        makeUnit(101, "Slime", game::battle::BattleSide::Enemy, 0, 20, std::string{"enemy.slime"})};
    evt.reward_summary = reward_summary;

    game::system::helpers::NotificationTimer notification_state{};
    processBattleEndedForGameScene(
        registry_,
        dispatcher_,
        &services_,
        active_battle_initial_item_stocks_,
        has_active_battle_item_stocks_,
        notification_state,
        evt);

    EXPECT_TRUE(stats_capture.changes.empty());
    EXPECT_EQ(runtime_stats.revision_, 0U);
    EXPECT_EQ(runtime_stats.states_by_actor_id_.at("actor.player").total_exp, max_exp);

    stats_sink.disconnect<&PartyRuntimeStatsCapture::onChanged>(&stats_capture);
}

TEST_F(GameSceneBattleRewardWritebackTest, VictoryWritesBackDeltaRewardsAndNotification) {
    DialogueCapture capture;
    auto sink = dispatcher_.sink<game::defs::DialogueShowEvent>();
    sink.connect<&DialogueCapture::onShow>(&capture);

    const entt::entity player = createPlayer(10, 3);
    const entt::id_type herb_id = game::data::RpgCatalog::hashId("item.herb");
    active_battle_initial_item_stocks_[herb_id] = 3;
    has_active_battle_item_stocks_ = true;

    game::defs::BattleEndedEvent evt{};
    evt.outcome = game::battle::BattleOutcome::Victory;
    evt.remaining_item_stocks = {{herb_id, 1}};
    evt.final_units = {
        makeUnit(1, "Hero", game::battle::BattleSide::Player, 30, 30),
        makeUnit(101, "Slime", game::battle::BattleSide::Enemy, 0, 20, std::string{"enemy.slime"})};
    evt.reward_summary = slimeRewardSummary();

    game::system::helpers::NotificationTimer notification_state{};
    processBattleEndedForGameScene(
        registry_,
        dispatcher_,
        &services_,
        active_battle_initial_item_stocks_,
        has_active_battle_item_stocks_,
        notification_state,
        evt);

    EXPECT_EQ(registry_.get<game::component::PlayerWalletComponent>(player).gold_, 14);
    EXPECT_EQ(countItem(player, herb_id), 2);
    EXPECT_FALSE(has_active_battle_item_stocks_);
    EXPECT_TRUE(active_battle_initial_item_stocks_.empty());
    EXPECT_EQ(notification_state.target, player);
    EXPECT_FLOAT_EQ(notification_state.remaining_seconds, 2.0F);

    ASSERT_EQ(capture.shows.size(), 1U);
    EXPECT_EQ(capture.shows[0].target, player);
    EXPECT_EQ(capture.shows[0].text, "Gained Gold 4\nGained Herb x1");
    EXPECT_FLOAT_EQ(capture.shows[0].world_position.x, 128.0F);
    EXPECT_FLOAT_EQ(capture.shows[0].world_position.y, 48.0F);
    EXPECT_EQ(capture.shows[0].channel, game::defs::DialogueChannel::Notice);

    sink.disconnect<&DialogueCapture::onShow>(&capture);
}

TEST_F(GameSceneBattleRewardWritebackTest, VictoryNotificationMovesAndAutoHidesAfterTimeout) {
    DialogueCapture capture;
    auto show_sink = dispatcher_.sink<game::defs::DialogueShowEvent>();
    auto move_sink = dispatcher_.sink<game::defs::DialogueMoveEvent>();
    auto hide_sink = dispatcher_.sink<game::defs::DialogueHideEvent>();
    show_sink.connect<&DialogueCapture::onShow>(&capture);
    move_sink.connect<&DialogueCapture::onMove>(&capture);
    hide_sink.connect<&DialogueCapture::onHide>(&capture);

    const entt::entity player = createPlayer(10, 0);

    game::defs::BattleEndedEvent evt{};
    evt.outcome = game::battle::BattleOutcome::Victory;
    evt.final_units = {
        makeUnit(1, "Hero", game::battle::BattleSide::Player, 30, 30),
        makeUnit(101, "Slime", game::battle::BattleSide::Enemy, 0, 20, std::string{"enemy.slime"})};
    evt.reward_summary = slimeRewardSummary();

    game::system::helpers::NotificationTimer notification_state{};
    processBattleEndedForGameScene(
        registry_,
        dispatcher_,
        &services_,
        active_battle_initial_item_stocks_,
        has_active_battle_item_stocks_,
        notification_state,
        evt);

    ASSERT_EQ(capture.shows.size(), 1U);
    ASSERT_EQ(notification_state.target, player);
    EXPECT_FLOAT_EQ(notification_state.remaining_seconds, 2.0F);

    auto& transform = registry_.get<engine::component::TransformComponent>(player);
    transform.position_ = {160.0f, 96.0f};

    game::system::helpers::updateTimedNotification(
        registry_, dispatcher_, game::defs::DialogueChannel::Notice, notification_state, 0.5F);
    EXPECT_EQ(capture.moves.size(), 1U);
    EXPECT_EQ(capture.moves[0].target, player);
    EXPECT_EQ(capture.moves[0].channel, game::defs::DialogueChannel::Notice);
    EXPECT_FLOAT_EQ(capture.moves[0].world_position.x, 160.0F);
    EXPECT_FLOAT_EQ(capture.moves[0].world_position.y, 80.0F);
    EXPECT_GT(notification_state.remaining_seconds, 0.0F);

    game::system::helpers::updateTimedNotification(
        registry_, dispatcher_, game::defs::DialogueChannel::Notice, notification_state, 1.5F);
    dispatcher_.update();

    EXPECT_EQ(capture.moves.size(), 2U);
    EXPECT_EQ(capture.hides.size(), 1U);
    EXPECT_EQ(capture.hides[0].target, player);
    EXPECT_EQ(capture.hides[0].channel, game::defs::DialogueChannel::Notice);
    EXPECT_TRUE(notification_state.target == entt::null);
    EXPECT_FLOAT_EQ(notification_state.remaining_seconds, 0.0F);

    hide_sink.disconnect<&DialogueCapture::onHide>(&capture);
    move_sink.disconnect<&DialogueCapture::onMove>(&capture);
    show_sink.disconnect<&DialogueCapture::onShow>(&capture);
}

TEST_F(GameSceneBattleRewardWritebackTest, DefeatOnlyWritesBackBattleItemDelta) {
    DialogueCapture capture;
    auto sink = dispatcher_.sink<game::defs::DialogueShowEvent>();
    sink.connect<&DialogueCapture::onShow>(&capture);

    const entt::entity player = createPlayer(10, 3);
    const entt::id_type herb_id = game::data::RpgCatalog::hashId("item.herb");
    active_battle_initial_item_stocks_[herb_id] = 3;
    has_active_battle_item_stocks_ = true;

    game::defs::BattleEndedEvent evt{};
    evt.outcome = game::battle::BattleOutcome::Defeat;
    evt.remaining_item_stocks = {{herb_id, 1}};
    evt.final_units = {
        makeUnit(1, "Hero", game::battle::BattleSide::Player, 0, 30),
        makeUnit(101, "Slime", game::battle::BattleSide::Enemy, 0, 20, std::string{"enemy.slime"})};

    game::system::helpers::NotificationTimer notification_state{};
    processBattleEndedForGameScene(
        registry_,
        dispatcher_,
        &services_,
        active_battle_initial_item_stocks_,
        has_active_battle_item_stocks_,
        notification_state,
        evt);
    EXPECT_TRUE(notification_state.target == entt::null);

    EXPECT_EQ(registry_.get<game::component::PlayerWalletComponent>(player).gold_, 10);
    EXPECT_EQ(countItem(player, herb_id), 1);
    EXPECT_FALSE(has_active_battle_item_stocks_);
    EXPECT_TRUE(active_battle_initial_item_stocks_.empty());
    EXPECT_TRUE(capture.shows.empty());

    sink.disconnect<&DialogueCapture::onShow>(&capture);
}

TEST_F(GameSceneBattleRewardWritebackTest, EscapedOnlyWritesBackBattleItemDelta) {
    DialogueCapture capture;
    auto sink = dispatcher_.sink<game::defs::DialogueShowEvent>();
    sink.connect<&DialogueCapture::onShow>(&capture);

    const entt::entity player = createPlayer(10, 3);
    const entt::id_type herb_id = game::data::RpgCatalog::hashId("item.herb");
    active_battle_initial_item_stocks_[herb_id] = 3;
    has_active_battle_item_stocks_ = true;

    game::defs::BattleEndedEvent evt{};
    evt.outcome = game::battle::BattleOutcome::Escaped;
    evt.remaining_item_stocks = {{herb_id, 1}};
    evt.final_units = {
        makeUnit(1, "Hero", game::battle::BattleSide::Player, 30, 30),
        makeUnit(101, "Slime", game::battle::BattleSide::Enemy, 12, 20, std::string{"enemy.slime"})};

    game::system::helpers::NotificationTimer notification_state{};
    processBattleEndedForGameScene(
        registry_,
        dispatcher_,
        &services_,
        active_battle_initial_item_stocks_,
        has_active_battle_item_stocks_,
        notification_state,
        evt);
    EXPECT_TRUE(notification_state.target == entt::null);

    EXPECT_EQ(registry_.get<game::component::PlayerWalletComponent>(player).gold_, 10);
    EXPECT_EQ(countItem(player, herb_id), 1);
    EXPECT_FALSE(has_active_battle_item_stocks_);
    EXPECT_TRUE(active_battle_initial_item_stocks_.empty());
    EXPECT_TRUE(capture.shows.empty());

    sink.disconnect<&DialogueCapture::onShow>(&capture);
}

TEST_F(GameSceneBattleRewardWritebackTest, VictoryReportsRejectedDropsWhenInventoryIsFull) {
    DialogueCapture capture;
    auto sink = dispatcher_.sink<game::defs::DialogueShowEvent>();
    sink.connect<&DialogueCapture::onShow>(&capture);

    const entt::entity player = createPlayer(10, 99, true);

    game::defs::BattleEndedEvent evt{};
    evt.outcome = game::battle::BattleOutcome::Victory;
    evt.final_units = {
        makeUnit(1, "Hero", game::battle::BattleSide::Player, 30, 30),
        makeUnit(101, "Slime", game::battle::BattleSide::Enemy, 0, 20, std::string{"enemy.slime"})};
    evt.reward_summary = slimeRewardSummary();

    game::system::helpers::NotificationTimer notification_state{};
    processBattleEndedForGameScene(
        registry_,
        dispatcher_,
        &services_,
        active_battle_initial_item_stocks_,
        has_active_battle_item_stocks_,
        notification_state,
        evt);
    EXPECT_EQ(notification_state.target, player);

    EXPECT_EQ(registry_.get<game::component::PlayerWalletComponent>(player).gold_, 14);
    EXPECT_EQ(countItem(player, game::data::RpgCatalog::hashId("item.herb")), 99);

    ASSERT_EQ(capture.shows.size(), 1U);
    EXPECT_EQ(capture.shows[0].text, "Gained Gold 4\nInventory full, missed Herb x1");

    sink.disconnect<&DialogueCapture::onShow>(&capture);
}

TEST_F(GameSceneBattleRewardWritebackTest, VictoryRejectsOverflowedGoldWithoutMutatingWallet) {
    DialogueCapture capture;
    auto sink = dispatcher_.sink<game::defs::DialogueShowEvent>();
    sink.connect<&DialogueCapture::onShow>(&capture);

    const int initial_gold = std::numeric_limits<int>::max() - 1;
    const entt::entity player = createPlayer(initial_gold, 0);

    game::defs::BattleEndedEvent evt{};
    evt.outcome = game::battle::BattleOutcome::Victory;
    evt.final_units = {
        makeUnit(1, "Hero", game::battle::BattleSide::Player, 30, 30),
        makeUnit(101, "Slime", game::battle::BattleSide::Enemy, 0, 20, std::string{"enemy.slime"})};
    evt.reward_summary = game::battle::BattleRewardSummary{
        .gold_total = 2,
    };

    game::system::helpers::NotificationTimer notification_state{};
    processBattleEndedForGameScene(
        registry_,
        dispatcher_,
        &services_,
        active_battle_initial_item_stocks_,
        has_active_battle_item_stocks_,
        notification_state,
        evt);

    EXPECT_EQ(registry_.get<game::component::PlayerWalletComponent>(player).gold_, initial_gold);
    ASSERT_EQ(capture.shows.size(), 1U);
    EXPECT_EQ(capture.shows[0].text, "Victory");

    sink.disconnect<&DialogueCapture::onShow>(&capture);
}

TEST_F(GameSceneBattleRewardWritebackTest, VictoryCombinesRewardAndQuestProgressIntoSingleNotification) {
    DialogueCapture capture;
    auto sink = dispatcher_.sink<game::defs::DialogueShowEvent>();
    sink.connect<&DialogueCapture::onShow>(&capture);

    const entt::entity player = createPlayer(10, 0);
    auto& quest_log = registry_.get<game::component::QuestLogComponent>(player);
    quest_log.active_quests.push_back("quest.slime_hunt");
    quest_log.objective_progress[game::data::makeQuestObjectiveProgressKey("quest.slime_hunt", "slime_count")] = 1;

    game::defs::BattleEndedEvent evt{};
    evt.outcome = game::battle::BattleOutcome::Victory;
    evt.final_units = {
        makeUnit(1, "Hero", game::battle::BattleSide::Player, 30, 30),
        makeUnit(101, "Slime", game::battle::BattleSide::Enemy, 0, 20, std::string{"enemy.slime"})};
    evt.reward_summary = slimeRewardSummary();

    game::system::helpers::NotificationTimer notification_state{};
    processBattleEndedForGameScene(
        registry_,
        dispatcher_,
        &services_,
        active_battle_initial_item_stocks_,
        has_active_battle_item_stocks_,
        notification_state,
        evt);

    EXPECT_EQ(quest_log.objective_progress[game::data::makeQuestObjectiveProgressKey("quest.slime_hunt", "slime_count")], 2);
    ASSERT_EQ(capture.shows.size(), 1U);
    EXPECT_EQ(capture.shows[0].target, player);
    EXPECT_EQ(capture.shows[0].channel, game::defs::DialogueChannel::Notice);
    EXPECT_EQ(capture.shows[0].text, "Gained Gold 4\nGained Herb x1\nReady: Slime Hunt");

    sink.disconnect<&DialogueCapture::onShow>(&capture);
}

} // namespace
} // namespace game::scene
// NOLINTEND
