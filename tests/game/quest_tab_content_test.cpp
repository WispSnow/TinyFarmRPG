#include <gtest/gtest.h>

#include "game/component/quest_log_component.h"
#include "game/data/quest_catalog.h"
#include "game/data/quest_data.h"
#include "game/ui/quest_tab_content.h"

#include <entt/entity/registry.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace {

[[nodiscard]] std::filesystem::path writeQuestConfig(std::string_view prefix, std::string_view body) {
    const auto temp_root = std::filesystem::temp_directory_path() /
                           (std::string(prefix) + "_" +
                            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(temp_root);
    const auto config_path = temp_root / "quests.json";

    std::ofstream file(config_path);
    EXPECT_TRUE(file.is_open()) << config_path;
    file << body;
    file.close();

    return config_path;
}

[[nodiscard]] game::data::QuestCatalog loadQuestCatalog() {
    const auto config_path = writeQuestConfig(
        "quest_tab_content",
        R"({
  "schema_version": 1,
  "quests": [
    {
      "id": "quest.active.multi",
      "title": "Hunter Trial",
      "description": "Defeat slimes and bats for the village.",
      "objectives": [
        {
          "id": "slime_hunt",
          "kind": "defeat_enemy_count",
          "enemy_id": "enemy.slime",
          "required_count": 2
        },
        {
          "id": "bat_hunt",
          "kind": "defeat_enemy_count",
          "enemy_id": "enemy.bat",
          "required_count": 1
        }
      ]
    },
    {
      "id": "quest.completed.single",
      "title": "Village Thanks",
      "description": "Already wrapped up.",
      "objectives": [
        {
          "id": "slime_hunt",
          "kind": "defeat_enemy_count",
          "enemy_id": "enemy.slime",
          "required_count": 1
        }
      ]
    }
  ]
})");

    game::data::QuestCatalog catalog;
    EXPECT_TRUE(catalog.loadFromFile(config_path.string()));
    std::error_code ec;
    std::filesystem::remove_all(config_path.parent_path(), ec);
    return catalog;
}

[[nodiscard]] entt::entity createPlayer(entt::registry& registry) {
    const entt::entity player = registry.create();
    registry.emplace<game::component::QuestLogComponent>(player);
    return player;
}

} // namespace

namespace game::ui {

TEST(QuestTabContentTest, BuildQuestTabViewStateMapsActiveAndCompletedEntries) {
    entt::registry registry;
    const entt::entity player = createPlayer(registry);
    auto& quest_log = registry.get<game::component::QuestLogComponent>(player);
    quest_log.active_quests = {"quest.active.multi"};
    quest_log.completed_quests = {"quest.completed.single"};
    quest_log.objective_progress[game::data::makeQuestObjectiveProgressKey("quest.active.multi", "slime_hunt")] = 1;
    quest_log.objective_progress[game::data::makeQuestObjectiveProgressKey("quest.active.multi", "bat_hunt")] = 0;

    const auto catalog = loadQuestCatalog();
    const QuestTabViewState state = buildQuestTabViewState(registry, player, &catalog);

    EXPECT_TRUE(state.has_active_quests);
    EXPECT_TRUE(state.has_completed_quests);
    ASSERT_EQ(state.active_quest_entries.size(), 1u);
    ASSERT_EQ(state.completed_quest_entries.size(), 1u);

    const auto& active = state.active_quest_entries.front();
    EXPECT_EQ(active.title, "Hunter Trial");
    EXPECT_EQ(active.description, "Defeat slimes and bats for the village.");
    EXPECT_EQ(active.status_label, "进行中");
    EXPECT_TRUE(active.has_description);
    EXPECT_TRUE(active.has_progress_summary);
    EXPECT_EQ(active.progress_summary, "Slime 1/2\nBat 0/1");

    const auto& completed = state.completed_quest_entries.front();
    EXPECT_EQ(completed.title, "Village Thanks");
    EXPECT_EQ(completed.status_label, "已完成");
    EXPECT_FALSE(completed.has_description);
    EXPECT_FALSE(completed.has_progress_summary);
    EXPECT_TRUE(completed.progress_summary.empty());
}

TEST(QuestTabContentTest, ReadyQuestUsesReadyLabelAndClampsProgressSummary) {
    entt::registry registry;
    const entt::entity player = createPlayer(registry);
    auto& quest_log = registry.get<game::component::QuestLogComponent>(player);
    quest_log.active_quests = {"quest.active.multi"};
    quest_log.objective_progress[game::data::makeQuestObjectiveProgressKey("quest.active.multi", "slime_hunt")] = 5;
    quest_log.objective_progress[game::data::makeQuestObjectiveProgressKey("quest.active.multi", "bat_hunt")] = 1;

    const auto catalog = loadQuestCatalog();
    const QuestTabViewState state = buildQuestTabViewState(registry, player, &catalog);

    ASSERT_EQ(state.active_quest_entries.size(), 1u);
    const auto& active = state.active_quest_entries.front();
    EXPECT_EQ(active.status_label, "可交付");
    EXPECT_EQ(active.progress_summary, "Slime 2/2\nBat 1/1");
}

TEST(QuestTabContentTest, MissingQuestDefinitionsAreSkippedSafely) {
    entt::registry registry;
    const entt::entity player = createPlayer(registry);
    auto& quest_log = registry.get<game::component::QuestLogComponent>(player);
    quest_log.active_quests = {"quest.missing"};
    quest_log.completed_quests = {"quest.also_missing"};

    const auto catalog = loadQuestCatalog();
    const QuestTabViewState state = buildQuestTabViewState(registry, player, &catalog);

    EXPECT_FALSE(state.has_active_quests);
    EXPECT_FALSE(state.has_completed_quests);
    EXPECT_TRUE(state.active_quest_entries.empty());
    EXPECT_TRUE(state.completed_quest_entries.empty());
}

TEST(QuestTabContentTest, EmptyQuestLogFallsBackToEmptyStates) {
    entt::registry registry;
    const entt::entity player = createPlayer(registry);
    const auto catalog = loadQuestCatalog();

    const QuestTabViewState state = buildQuestTabViewState(registry, player, &catalog);

    EXPECT_FALSE(state.has_active_quests);
    EXPECT_FALSE(state.has_completed_quests);
}

} // namespace game::ui
