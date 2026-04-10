#include <gtest/gtest.h>

#include "appearance_test_fixture_utils.h"
#include "game/data/item_catalog.h"
#include "game/data/quest_catalog.h"
#include "game/data/rpg_catalog.h"

#include <filesystem>
#include <string>
#include <string_view>

namespace game::data {
namespace {

[[nodiscard]] std::filesystem::path writeQuestConfig(std::string_view prefix, std::string_view body) {
    const auto temp_root = game::test::createUniqueTempDir(prefix);
    const auto config_path = temp_root / "quests.json";
    game::test::writeTextFile(config_path, body);
    return config_path;
}

[[nodiscard]] bool loadProjectItemCatalog(ItemCatalog& catalog) {
    const auto root = std::filesystem::path{PROJECT_SOURCE_DIR};
    return catalog.loadItemConfig((root / "assets/data/item_config.json").string());
}

[[nodiscard]] bool loadProjectRpgCatalog(RpgCatalog& catalog) {
    const auto root = std::filesystem::path{PROJECT_SOURCE_DIR};
    const auto rpg_root = root / "assets/data/rpg";
    return catalog.loadManifest((rpg_root / "manifest.json").string()) &&
           catalog.loadClasses((rpg_root / "classes.json").string()) &&
           catalog.loadActors((rpg_root / "actors.json").string()) &&
           catalog.loadSkills((rpg_root / "skills.json").string()) &&
           catalog.loadStates((rpg_root / "states.json").string()) &&
           catalog.loadEnemies((rpg_root / "enemies.json").string()) &&
           catalog.loadTroops((rpg_root / "troops.json").string());
}

TEST(QuestCatalogTest, LoadsProjectQuestAssetAndValidatesReferences) {
    const auto root = std::filesystem::path{PROJECT_SOURCE_DIR};
    QuestCatalog catalog;
    ASSERT_TRUE(catalog.loadFromFile((root / "assets/data/quests.json").string()));
    EXPECT_EQ(catalog.schemaVersion(), 1U);

    const auto* quest = catalog.findQuest("quest.village.goblin_cleanup");
    ASSERT_NE(quest, nullptr);
    EXPECT_EQ(quest->title_, "Goblin Cleanup");
    ASSERT_EQ(quest->objectives_.size(), 1U);
    EXPECT_EQ(quest->objectives_[0].id_, "kill_goblins");
    EXPECT_EQ(quest->objectives_[0].enemy_id_, "enemy.goblin");
    EXPECT_EQ(quest->objectives_[0].required_count_, 3);

    ItemCatalog item_catalog;
    ASSERT_TRUE(loadProjectItemCatalog(item_catalog));

    RpgCatalog rpg_catalog;
    ASSERT_TRUE(loadProjectRpgCatalog(rpg_catalog));

    std::string error{};
    EXPECT_TRUE(catalog.validateReferences(&rpg_catalog, &item_catalog, error)) << error;
}

TEST(QuestCatalogTest, RejectsDuplicateQuestId) {
    const auto path = writeQuestConfig(
        "quest_catalog_duplicate_quest_id",
        R"json({
  "schema_version": 1,
  "quests": [
    {
      "id": "quest.alpha",
      "title": "Alpha",
      "objectives": [
        {
          "id": "obj.one",
          "kind": "defeat_enemy_count",
          "enemy_id": "enemy.goblin",
          "required_count": 1
        }
      ]
    },
    {
      "id": "quest.alpha",
      "title": "Beta",
      "objectives": [
        {
          "id": "obj.two",
          "kind": "defeat_enemy_count",
          "enemy_id": "enemy.goblin",
          "required_count": 1
        }
      ]
    }
  ]
})json");

    QuestCatalog catalog;
    EXPECT_FALSE(catalog.loadFromFile(path.string()));
}

TEST(QuestCatalogTest, RejectsDuplicateObjectiveIdWithinQuest) {
    const auto path = writeQuestConfig(
        "quest_catalog_duplicate_objective_id",
        R"json({
  "schema_version": 1,
  "quests": [
    {
      "id": "quest.alpha",
      "title": "Alpha",
      "objectives": [
        {
          "id": "obj.same",
          "kind": "defeat_enemy_count",
          "enemy_id": "enemy.goblin",
          "required_count": 1
        },
        {
          "id": "obj.same",
          "kind": "defeat_enemy_count",
          "enemy_id": "enemy.goblin",
          "required_count": 2
        }
      ]
    }
  ]
})json");

    QuestCatalog catalog;
    EXPECT_FALSE(catalog.loadFromFile(path.string()));
}

TEST(QuestCatalogTest, RejectsReservedSeparatorInQuestId) {
    const auto path = writeQuestConfig(
        "quest_catalog_reserved_separator_quest",
        R"json({
  "schema_version": 1,
  "quests": [
    {
      "id": "quest::alpha",
      "title": "Alpha",
      "objectives": [
        {
          "id": "obj.one",
          "kind": "defeat_enemy_count",
          "enemy_id": "enemy.goblin",
          "required_count": 1
        }
      ]
    }
  ]
})json");

    QuestCatalog catalog;
    EXPECT_FALSE(catalog.loadFromFile(path.string()));
}

TEST(QuestCatalogTest, RejectsReservedSeparatorInObjectiveId) {
    const auto path = writeQuestConfig(
        "quest_catalog_reserved_separator_objective",
        R"json({
  "schema_version": 1,
  "quests": [
    {
      "id": "quest.alpha",
      "title": "Alpha",
      "objectives": [
        {
          "id": "obj::one",
          "kind": "defeat_enemy_count",
          "enemy_id": "enemy.goblin",
          "required_count": 1
        }
      ]
    }
  ]
})json");

    QuestCatalog catalog;
    EXPECT_FALSE(catalog.loadFromFile(path.string()));
}

TEST(QuestCatalogTest, RejectsMissingTitle) {
    const auto path = writeQuestConfig(
        "quest_catalog_missing_title",
        R"json({
  "schema_version": 1,
  "quests": [
    {
      "id": "quest.alpha",
      "objectives": [
        {
          "id": "obj.one",
          "kind": "defeat_enemy_count",
          "enemy_id": "enemy.goblin",
          "required_count": 1
        }
      ]
    }
  ]
})json");

    QuestCatalog catalog;
    EXPECT_FALSE(catalog.loadFromFile(path.string()));
}

TEST(QuestCatalogTest, RejectsNonStringDescription) {
    const auto path = writeQuestConfig(
        "quest_catalog_non_string_description",
        R"json({
  "schema_version": 1,
  "quests": [
    {
      "id": "quest.alpha",
      "title": "Alpha",
      "description": 123,
      "objectives": [
        {
          "id": "obj.one",
          "kind": "defeat_enemy_count",
          "enemy_id": "enemy.goblin",
          "required_count": 1
        }
      ]
    }
  ]
})json");

    QuestCatalog catalog;
    EXPECT_FALSE(catalog.loadFromFile(path.string()));
}

TEST(QuestCatalogTest, ValidateReferencesFailsOnUnknownEnemy) {
    const auto path = writeQuestConfig(
        "quest_catalog_missing_enemy",
        R"json({
  "schema_version": 1,
  "quests": [
    {
      "id": "quest.alpha",
      "title": "Alpha",
      "objectives": [
        {
          "id": "obj.one",
          "kind": "defeat_enemy_count",
          "enemy_id": "enemy.missing",
          "required_count": 1
        }
      ]
    }
  ]
})json");

    QuestCatalog catalog;
    ASSERT_TRUE(catalog.loadFromFile(path.string()));

    ItemCatalog item_catalog;
    ASSERT_TRUE(loadProjectItemCatalog(item_catalog));

    RpgCatalog rpg_catalog;
    ASSERT_TRUE(loadProjectRpgCatalog(rpg_catalog));

    std::string error{};
    EXPECT_FALSE(catalog.validateReferences(&rpg_catalog, &item_catalog, error));
    EXPECT_NE(error.find("enemy.missing"), std::string::npos);
}

TEST(QuestCatalogTest, ValidateReferencesFailsOnUnknownRewardItem) {
    const auto path = writeQuestConfig(
        "quest_catalog_missing_reward_item",
        R"json({
  "schema_version": 1,
  "quests": [
    {
      "id": "quest.alpha",
      "title": "Alpha",
      "objectives": [
        {
          "id": "obj.one",
          "kind": "defeat_enemy_count",
          "enemy_id": "enemy.goblin",
          "required_count": 1
        }
      ],
      "rewards": {
        "items": [
          {
            "item_id": "item.missing",
            "count": 1
          }
        ]
      }
    }
  ]
})json");

    QuestCatalog catalog;
    ASSERT_TRUE(catalog.loadFromFile(path.string()));

    ItemCatalog item_catalog;
    ASSERT_TRUE(loadProjectItemCatalog(item_catalog));

    RpgCatalog rpg_catalog;
    ASSERT_TRUE(loadProjectRpgCatalog(rpg_catalog));

    std::string error{};
    EXPECT_FALSE(catalog.validateReferences(&rpg_catalog, &item_catalog, error));
    EXPECT_NE(error.find("item.missing"), std::string::npos);
}

TEST(QuestCatalogTest, MakeQuestObjectiveProgressKeyBuildsStableCompositeKey) {
    EXPECT_EQ(
        makeQuestObjectiveProgressKey("quest.village.goblin_cleanup", "kill_goblins"),
        "quest.village.goblin_cleanup::kill_goblins");
}

} // namespace
} // namespace game::data
