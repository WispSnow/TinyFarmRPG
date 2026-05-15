#include <gtest/gtest.h>

#include "appearance_test_fixture_utils.h"
#include "game/data/quest_catalog.h"
#include "game/ui/quest_map_marker_resolver.h"

#include <entt/core/hashed_string.hpp>

#include <filesystem>
#include <string_view>
#include <vector>

using namespace entt::literals;

namespace game::ui {
namespace {

[[nodiscard]] std::filesystem::path writeQuestConfig(std::string_view prefix, std::string_view body) {
    const auto temp_root = game::test::createUniqueTempDir(prefix);
    const auto config_path = temp_root / "quests.json";
    game::test::writeTextFile(config_path, body);
    return config_path;
}

[[nodiscard]] game::data::QuestCatalog loadMarkerQuestCatalog() {
    const auto path = writeQuestConfig(
        "quest_map_marker_resolver_catalog",
        R"json({
  "schema_version": 1,
  "quests": [
    {
      "id": "quest.alpha",
      "title": "Alpha Quest",
      "description": "Help the village.",
      "objectives": [
        {
          "id": "kill_goblins",
          "kind": "defeat_enemy_count",
          "enemy_id": "enemy.goblin",
          "required_count": 3,
          "marker": {
            "map": "town",
            "position": [120, 64],
            "label": "Goblin Nest"
          }
        }
      ]
    },
    {
      "id": "quest.beta",
      "title": "Beta Quest",
      "objectives": [
        {
          "id": "find_bats",
          "kind": "defeat_enemy_count",
          "enemy_id": "enemy.bat",
          "required_count": 2,
          "marker": {
            "map": "town",
            "position": [120, 64]
          }
        }
      ]
    }
  ]
})json");

    game::data::QuestCatalog catalog;
    EXPECT_TRUE(catalog.loadFromFile(path.string()));
    return catalog;
}

[[nodiscard]] std::vector<QuestGiverLocation> makeQuestGivers() {
    return std::vector<QuestGiverLocation>{
        QuestGiverLocation{
            .object_id = 10,
            .object_name = "alpha_giver",
            .quest_id = "quest.alpha",
            .map_position = {32.0F, 48.0F},
        },
        QuestGiverLocation{
            .object_id = 11,
            .object_name = "beta_giver",
            .quest_id = "quest.beta",
            .map_position = {64.0F, 48.0F},
        },
    };
}

TEST(QuestMapMarkerResolverTest, ShowsOfferableGiverMarkersForInactiveQuests) {
    const game::data::QuestCatalog catalog = loadMarkerQuestCatalog();
    const game::component::QuestLogComponent quest_log{};
    const std::vector<QuestGiverLocation> givers = makeQuestGivers();

    const std::vector<QuestRuntimeMarker> markers =
        resolveQuestMapMarkers("home"_hs, quest_log, catalog, givers);

    ASSERT_EQ(markers.size(), 2U);
    EXPECT_EQ(markers[0].kind, QuestRuntimeMarkerKind::Offer);
    EXPECT_EQ(markers[0].quest_id, "quest.alpha");
    EXPECT_EQ(markers[0].title, "Alpha Quest");
    EXPECT_EQ(markers[0].type_label, "Quest Available");
    EXPECT_EQ(markers[0].description, "Help the village.");
    EXPECT_EQ(markers[1].quest_id, "quest.beta");
}

TEST(QuestMapMarkerResolverTest, ShowsActiveObjectiveMarkersOnCurrentMap) {
    const game::data::QuestCatalog catalog = loadMarkerQuestCatalog();
    game::component::QuestLogComponent quest_log{};
    quest_log.active_quests = {"quest.alpha"};
    quest_log.objective_progress[game::data::makeQuestObjectiveProgressKey("quest.alpha", "kill_goblins")] = 1;

    const std::vector<QuestRuntimeMarker> markers =
        resolveQuestMapMarkers("town"_hs, quest_log, catalog, {});

    ASSERT_EQ(markers.size(), 1U);
    EXPECT_EQ(markers[0].kind, QuestRuntimeMarkerKind::Objective);
    EXPECT_EQ(markers[0].quest_id, "quest.alpha");
    EXPECT_EQ(markers[0].objective_id, "kill_goblins");
    EXPECT_EQ(markers[0].title, "Goblin Nest");
    EXPECT_EQ(markers[0].type_label, "Quest Objective");
    EXPECT_EQ(markers[0].description, "Goblin 1/3");
    EXPECT_EQ(markers[0].map_position.x, 120.0F);
    EXPECT_EQ(markers[0].map_position.y, 64.0F);
}

TEST(QuestMapMarkerResolverTest, TreatsMissingProgressKeyAsZeroWithoutWritingQuestLog) {
    const game::data::QuestCatalog catalog = loadMarkerQuestCatalog();
    game::component::QuestLogComponent quest_log{};
    quest_log.active_quests = {"quest.alpha"};

    const std::vector<QuestRuntimeMarker> markers =
        resolveQuestMapMarkers("town"_hs, quest_log, catalog, {});

    ASSERT_EQ(markers.size(), 1U);
    EXPECT_EQ(markers[0].description, "Goblin 0/3");
    EXPECT_TRUE(quest_log.objective_progress.empty());
}

TEST(QuestMapMarkerResolverTest, ReadyQuestShowsTurnInMarkerAndHidesObjectiveMarker) {
    const game::data::QuestCatalog catalog = loadMarkerQuestCatalog();
    game::component::QuestLogComponent quest_log{};
    quest_log.active_quests = {"quest.alpha"};
    quest_log.objective_progress[game::data::makeQuestObjectiveProgressKey("quest.alpha", "kill_goblins")] = 3;
    const std::vector<QuestGiverLocation> givers = makeQuestGivers();

    const std::vector<QuestRuntimeMarker> home_markers =
        resolveQuestMapMarkers("home"_hs, quest_log, catalog, givers);
    const std::vector<QuestRuntimeMarker> town_markers =
        resolveQuestMapMarkers("town"_hs, quest_log, catalog, {});

    ASSERT_EQ(home_markers.size(), 2U);
    EXPECT_EQ(home_markers[0].kind, QuestRuntimeMarkerKind::TurnIn);
    EXPECT_EQ(home_markers[0].quest_id, "quest.alpha");
    EXPECT_EQ(home_markers[0].type_label, "Ready to Turn In");
    EXPECT_EQ(home_markers[0].description, "Return to the quest giver.");
    EXPECT_EQ(home_markers[1].kind, QuestRuntimeMarkerKind::Offer);
    EXPECT_TRUE(town_markers.empty());
}

TEST(QuestMapMarkerResolverTest, CompletedQuestDoesNotShowAnyQuestMarker) {
    const game::data::QuestCatalog catalog = loadMarkerQuestCatalog();
    game::component::QuestLogComponent quest_log{};
    quest_log.completed_quests = {"quest.alpha"};
    const std::vector<QuestGiverLocation> givers = makeQuestGivers();

    const std::vector<QuestRuntimeMarker> markers =
        resolveQuestMapMarkers("home"_hs, quest_log, catalog, givers);

    ASSERT_EQ(markers.size(), 1U);
    EXPECT_EQ(markers[0].quest_id, "quest.beta");
}

TEST(QuestMapMarkerResolverTest, FiltersObjectiveMarkersOutsideCurrentMapAndUsesTitleFallback) {
    const game::data::QuestCatalog catalog = loadMarkerQuestCatalog();
    game::component::QuestLogComponent quest_log{};
    quest_log.active_quests = {"quest.beta"};

    const std::vector<QuestRuntimeMarker> home_markers =
        resolveQuestMapMarkers("home"_hs, quest_log, catalog, {});
    const std::vector<QuestRuntimeMarker> town_markers =
        resolveQuestMapMarkers("town"_hs, quest_log, catalog, {});

    EXPECT_TRUE(home_markers.empty());
    ASSERT_EQ(town_markers.size(), 1U);
    EXPECT_EQ(town_markers[0].title, "Find Bats");
    EXPECT_EQ(town_markers[0].description, "Bat 0/2");
}

} // namespace
} // namespace game::ui
