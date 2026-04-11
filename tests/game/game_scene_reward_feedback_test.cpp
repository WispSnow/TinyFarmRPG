// NOLINTBEGIN
#include <gtest/gtest.h>

#include "appearance_test_fixture_utils.h"
#include "game/data/item_catalog.h"
#include "game/data/rpg_catalog.h"
#include "game/scene/game_scene_reward_feedback.h"

#include <filesystem>
#include <string_view>
#include <vector>

namespace game::scene {
namespace {

[[nodiscard]] std::filesystem::path writeItemConfig(std::string_view prefix, std::string_view body) {
    const auto temp_root = game::test::createUniqueTempDir(prefix);
    const auto config_path = temp_root / "items.json";
    game::test::writeTextFile(config_path, body);
    return config_path;
}

TEST(GameSceneRewardFeedbackTest, FormatsGoldAcceptedAndRejectedItems) {
    const auto config_path = writeItemConfig(
        "game_scene_reward_feedback",
        R"json({
  "items": [
    {
      "id": "item.herb",
      "display_name": "Herb",
      "category": "material"
    }
  ]
})json");

    game::data::ItemCatalog catalog;
    ASSERT_TRUE(catalog.loadItemConfig(config_path.string()));

    const std::vector<BattleRewardWritebackItemResult> item_results{
        BattleRewardWritebackItemResult{
            .drop = game::battle::BattleRewardItemDrop{
                .item_id = "item.herb",
                .item_id_hash = game::data::RpgCatalog::hashId("item.herb"),
                .count = 3},
            .accepted = 2,
            .rejected = 1},
        BattleRewardWritebackItemResult{
            .drop = game::battle::BattleRewardItemDrop{
                .item_id = "item.mystery",
                .item_id_hash = game::data::RpgCatalog::hashId("item.mystery"),
                .count = 1},
            .accepted = 1,
            .rejected = 0}};

    const std::string feedback = formatRewardFeedback(15, item_results, &catalog);

    EXPECT_EQ(feedback,
              "获得金币 15\n"
              "获得 Herb x2\n"
              "背包已满，未获得 Herb x1\n"
              "获得 item.mystery x1");
}

TEST(GameSceneRewardFeedbackTest, ReturnsVictoryTextWhenNothingWasWrittenBack) {
    EXPECT_EQ(formatRewardFeedback(0, {}, nullptr), "战斗胜利");
}

TEST(GameSceneRewardFeedbackTest, FormatsBattleSettlementFeedbackWithQuestLines) {
    BattleRewardWritebackResult reward_result{};
    reward_result.gold_written_back = 4;

    game::domain::QuestBattleProgressSummary quest_summary{};
    quest_summary.updated_quests.push_back(game::domain::QuestBattleProgressQuestEntry{
        .quest_id = "quest.alpha",
        .quest_id_hash = game::data::RpgCatalog::hashId("quest.alpha"),
        .quest_title = "Alpha"});
    quest_summary.updated_quests.push_back(game::domain::QuestBattleProgressQuestEntry{
        .quest_id = "quest.beta",
        .quest_id_hash = game::data::RpgCatalog::hashId("quest.beta"),
        .quest_title = "Beta"});
    quest_summary.became_ready_to_turn_in_quests.push_back(game::domain::QuestBattleProgressQuestEntry{
        .quest_id = "quest.beta",
        .quest_id_hash = game::data::RpgCatalog::hashId("quest.beta"),
        .quest_title = "Beta"});

    const std::string feedback = formatBattleSettlementFeedback(reward_result, quest_summary, nullptr);

    EXPECT_EQ(feedback,
              "获得金币 4\n"
              "任务更新：Alpha\n"
              "可交付：Beta");
}

TEST(GameSceneRewardFeedbackTest, FormatsQuestOnlyFeedbackWithoutVictoryFallbackPrefix) {
    BattleRewardWritebackResult reward_result{};
    game::domain::QuestBattleProgressSummary quest_summary{};
    quest_summary.became_ready_to_turn_in_quests.push_back(game::domain::QuestBattleProgressQuestEntry{
        .quest_id = "quest.beta",
        .quest_id_hash = game::data::RpgCatalog::hashId("quest.beta"),
        .quest_title = "Beta"});

    const std::string feedback = formatBattleSettlementFeedback(reward_result, quest_summary, nullptr);

    EXPECT_EQ(feedback, "可交付：Beta");
}

} // namespace
} // namespace game::scene
// NOLINTEND
