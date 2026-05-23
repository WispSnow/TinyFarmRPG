// NOLINTBEGIN
#include <gtest/gtest.h>

#include "appearance_test_fixture_utils.h"
#include "game/battle/battle_unit_factory.h"
#include "game/component/quest_log_component.h"
#include "game/data/quest_catalog.h"
#include "game/data/rpg_catalog.h"
#include "game/domain/quest_battle_progress_resolver.h"
#include "game/domain/quest_log_ops.h"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace game::domain {
namespace {

[[nodiscard]] std::filesystem::path writeQuestConfig(std::string_view prefix, std::string_view body) {
    const auto temp_root = game::test::createUniqueTempDir(prefix);
    const auto config_path = temp_root / "quests.json";
    game::test::writeTextFile(config_path, body);
    return config_path;
}

[[nodiscard]] game::battle::BattleUnit makeUnit(const game::battle::BattleUnitId id,
                                                std::string_view name,
                                                const game::battle::BattleSide side,
                                                const int hp,
                                                const int max_hp,
                                                std::optional<std::string> source_enemy_id = std::nullopt) {
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
        .source_enemy_id = std::move(source_enemy_id)};
}

[[nodiscard]] game::data::QuestCatalog loadQuestCatalog() {
    const auto config_path = writeQuestConfig(
        "quest_battle_progress_resolver",
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
          "required_count": 3
        }
      ]
    },
    {
      "id": "quest.night_watch",
      "title": "Night Watch",
      "objectives": [
        {
          "id": "slime_count",
          "kind": "defeat_enemy_count",
          "enemy_id": "enemy.slime",
          "required_count": 1
        },
        {
          "id": "bat_count",
          "kind": "defeat_enemy_count",
          "enemy_id": "enemy.bat",
          "required_count": 1
        }
      ]
    },
    {
      "id": "quest.completed_hunt",
      "title": "Completed Hunt",
      "objectives": [
        {
          "id": "slime_count",
          "kind": "defeat_enemy_count",
          "enemy_id": "enemy.slime",
          "required_count": 1
        }
      ]
    }
  ]
})json");

    game::data::QuestCatalog catalog;
    EXPECT_TRUE(catalog.loadFromFile(config_path.string()));
    return catalog;
}

[[nodiscard]] std::vector<std::string> collectQuestIds(const std::vector<QuestBattleProgressQuestEntry>& entries) {
    std::vector<std::string> ids{};
    ids.reserve(entries.size());
    for (const auto& entry : entries) {
        ids.push_back(entry.quest_id);
    }
    return ids;
}

[[nodiscard]] game::data::QuestCatalog loadProjectQuestCatalog() {
    const auto quest_path = (std::filesystem::path{PROJECT_SOURCE_DIR} / "assets/data/quests.json").lexically_normal();
    game::data::QuestCatalog catalog;
    EXPECT_TRUE(catalog.loadFromFile(quest_path.string()));
    return catalog;
}

[[nodiscard]] game::data::RpgCatalog loadProjectRpgCatalog() {
    const auto rpg_root = (std::filesystem::path{PROJECT_SOURCE_DIR} / "assets/data/rpg").lexically_normal();
    game::data::RpgCatalog catalog;
    EXPECT_TRUE(catalog.loadClasses((rpg_root / "classes.json").string()));
    EXPECT_TRUE(catalog.loadActors((rpg_root / "actors.json").string()));
    EXPECT_TRUE(catalog.loadSkills((rpg_root / "skills.json").string()));
    EXPECT_TRUE(catalog.loadStates((rpg_root / "states.json").string()));
    EXPECT_TRUE(catalog.loadEnemies((rpg_root / "enemies.json").string()));
    EXPECT_TRUE(catalog.loadTroops((rpg_root / "troops.json").string()));
    return catalog;
}

TEST(QuestBattleProgressResolverTest, VictoryAggregatesKillsUpdatesProgressAndMarksReadyQuests) {
    auto catalog = loadQuestCatalog();
    QuestBattleProgressResolver resolver{};
    game::component::QuestLogComponent quest_log{};
    quest_log.active_quests = {"quest.slime_hunt", "quest.night_watch"};
    quest_log.objective_progress[game::data::makeQuestObjectiveProgressKey("quest.slime_hunt", "slime_count")] = 1;

    const auto summary = resolver.apply(
        game::battle::BattleOutcome::Victory,
        {
            makeUnit(1, "Hero", game::battle::BattleSide::Player, 30, 30),
            makeUnit(101, "Slime A", game::battle::BattleSide::Enemy, 0, 20, std::string{"enemy.slime"}),
            makeUnit(102, "Slime B", game::battle::BattleSide::Enemy, 0, 20, std::string{"enemy.slime"}),
            makeUnit(103, "Bat", game::battle::BattleSide::Enemy, 0, 20, std::string{"enemy.bat"}),
            makeUnit(104, "Living Slime", game::battle::BattleSide::Enemy, 5, 20, std::string{"enemy.slime"}),
            makeUnit(105, "No Source", game::battle::BattleSide::Enemy, 0, 20),
        },
        catalog,
        quest_log);

    EXPECT_EQ(quest_log.objective_progress[game::data::makeQuestObjectiveProgressKey("quest.slime_hunt", "slime_count")], 3);
    EXPECT_EQ(quest_log.objective_progress[game::data::makeQuestObjectiveProgressKey("quest.night_watch", "slime_count")], 1);
    EXPECT_EQ(quest_log.objective_progress[game::data::makeQuestObjectiveProgressKey("quest.night_watch", "bat_count")], 1);

    const auto updated_ids = collectQuestIds(summary.updated_quests);
    EXPECT_EQ(updated_ids, (std::vector<std::string>{"quest.slime_hunt", "quest.night_watch"}));

    const auto ready_ids = collectQuestIds(summary.became_ready_to_turn_in_quests);
    EXPECT_EQ(ready_ids, (std::vector<std::string>{"quest.slime_hunt", "quest.night_watch"}));
}

TEST(QuestBattleProgressResolverTest, NonVictoryDoesNotMutateQuestLog) {
    auto catalog = loadQuestCatalog();
    QuestBattleProgressResolver resolver{};
    game::component::QuestLogComponent quest_log{};
    quest_log.active_quests = {"quest.slime_hunt"};
    quest_log.objective_progress[game::data::makeQuestObjectiveProgressKey("quest.slime_hunt", "slime_count")] = 1;

    const auto summary = resolver.apply(
        game::battle::BattleOutcome::Defeat,
        {makeUnit(101, "Slime", game::battle::BattleSide::Enemy, 0, 20, std::string{"enemy.slime"})},
        catalog,
        quest_log);

    EXPECT_TRUE(summary.empty());
    EXPECT_EQ(quest_log.objective_progress[game::data::makeQuestObjectiveProgressKey("quest.slime_hunt", "slime_count")], 1);
}

TEST(QuestBattleProgressResolverTest, CompletedAndMissingQuestsAreSkipped) {
    auto catalog = loadQuestCatalog();
    QuestBattleProgressResolver resolver{};
    game::component::QuestLogComponent quest_log{};
    quest_log.active_quests = {"quest.completed_hunt", "quest.missing"};
    quest_log.completed_quests = {"quest.completed_hunt"};

    const auto summary = resolver.apply(
        game::battle::BattleOutcome::Victory,
        {makeUnit(101, "Slime", game::battle::BattleSide::Enemy, 0, 20, std::string{"enemy.slime"})},
        catalog,
        quest_log);

    EXPECT_TRUE(summary.empty());
    EXPECT_TRUE(quest_log.objective_progress.empty());
}

TEST(QuestBattleProgressResolverTest, ProjectSlimeTroopBattleAdvancesSlimeCleanupQuest) {
    auto quest_catalog = loadProjectQuestCatalog();
    auto rpg_catalog = loadProjectRpgCatalog();
    QuestBattleProgressResolver resolver{};

    std::vector<game::battle::BattleUnit> units{};
    std::string build_error{};
    ASSERT_TRUE(game::battle::buildBattleUnitsFromCatalog(
        rpg_catalog,
        game::battle::BattleUnitBuildOptions{.troop_id = "troop.slime"},
        units,
        build_error)) << build_error;

    int slime_enemy_count = 0;
    for (auto& unit : units) {
        if (unit.side != game::battle::BattleSide::Enemy) {
            continue;
        }
        ++slime_enemy_count;
        unit.hp = 0;
        ASSERT_TRUE(unit.source_enemy_id.has_value());
        EXPECT_EQ(*unit.source_enemy_id, "enemy.slime");
    }
    ASSERT_EQ(slime_enemy_count, 3);

    const auto* quest = quest_catalog.findQuest("quest.village.goblin_cleanup");
    ASSERT_NE(quest, nullptr);

    game::component::QuestLogComponent quest_log{};
    ASSERT_TRUE(game::domain::quest_log_ops::tryAcceptQuest(quest_log, *quest));
    quest_log.objective_progress[game::data::makeQuestObjectiveProgressKey(quest->id_, quest->objectives_.front().id_)] = 0;

    const auto summary = resolver.apply(
        game::battle::BattleOutcome::Victory,
        units,
        quest_catalog,
        quest_log);

    EXPECT_EQ(
        quest_log.objective_progress[game::data::makeQuestObjectiveProgressKey(quest->id_, quest->objectives_.front().id_)],
        3);

    const auto updated_ids = collectQuestIds(summary.updated_quests);
    EXPECT_EQ(updated_ids, (std::vector<std::string>{quest->id_}));

    const auto ready_ids = collectQuestIds(summary.became_ready_to_turn_in_quests);
    EXPECT_EQ(ready_ids, (std::vector<std::string>{quest->id_}));
}

} // namespace
} // namespace game::domain
// NOLINTEND
