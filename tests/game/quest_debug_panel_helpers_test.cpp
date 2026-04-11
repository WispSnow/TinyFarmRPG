// NOLINTBEGIN
#include <gtest/gtest.h>

#include "game/component/quest_log_component.h"
#include "game/data/quest_catalog.h"
#include "game/data/quest_data.h"
#include "game/debug/quest_debug_panel_helpers.h"

#include <string>
#include <string_view>

namespace {

[[nodiscard]] game::data::QuestData makeQuest(std::string_view quest_id,
                                              std::string_view objective_id = "slime_hunt",
                                              int required_count = 3) {
    game::data::QuestData quest{};
    quest.id_ = std::string(quest_id);
    quest.id_hash_ = game::data::QuestCatalog::hashId(quest.id_);
    quest.title_ = "Test Quest";
    quest.objectives_.push_back(game::data::QuestObjectiveData{
        .id_ = std::string(objective_id),
        .enemy_id_ = "slime",
        .required_count_ = required_count});
    return quest;
}

} // namespace

namespace game::debug {
namespace {

TEST(QuestDebugPanelHelpersTest, OfferableStateMeansQuestIsNeitherActiveNorCompleted) {
    const auto quest = makeQuest("quest.debug.offerable");
    game::component::QuestLogComponent quest_log{};

    const auto state = detail::resolveQuestDebugState(quest_log, quest);

    EXPECT_EQ(state, QuestDebugState::Offerable);
    EXPECT_TRUE(detail::matchesQuestDebugFilter(state, QuestDebugFilter::All));
    EXPECT_TRUE(detail::matchesQuestDebugFilter(state, QuestDebugFilter::Offerable));
    EXPECT_FALSE(detail::matchesQuestDebugFilter(state, QuestDebugFilter::Active));
    EXPECT_FALSE(detail::matchesQuestDebugFilter(state, QuestDebugFilter::Completed));
}

TEST(QuestDebugPanelHelpersTest, ReadyToTurnInStateOverridesActiveWhenObjectivesAreSatisfied) {
    const auto quest = makeQuest("quest.debug.ready");
    game::component::QuestLogComponent quest_log{};
    quest_log.active_quests.push_back(quest.id_);
    quest_log.objective_progress[game::data::makeQuestObjectiveProgressKey(quest.id_, quest.objectives_.front().id_)] =
        quest.objectives_.front().required_count_;

    const auto state = detail::resolveQuestDebugState(quest_log, quest);

    EXPECT_EQ(state, QuestDebugState::ReadyToTurnIn);
    EXPECT_TRUE(detail::matchesQuestDebugFilter(state, QuestDebugFilter::ReadyToTurnIn));
    EXPECT_FALSE(detail::matchesQuestDebugFilter(state, QuestDebugFilter::Active));
}

TEST(QuestDebugPanelHelpersTest, ActiveStateIsUsedWhenQuestIsAcceptedButObjectivesAreNotMet) {
    const auto quest = makeQuest("quest.debug.active");
    game::component::QuestLogComponent quest_log{};
    quest_log.active_quests.push_back(quest.id_);
    quest_log.objective_progress[game::data::makeQuestObjectiveProgressKey(quest.id_, quest.objectives_.front().id_)] = 1;

    const auto state = detail::resolveQuestDebugState(quest_log, quest);

    EXPECT_EQ(state, QuestDebugState::Active);
    EXPECT_TRUE(detail::matchesQuestDebugFilter(state, QuestDebugFilter::Active));
    EXPECT_FALSE(detail::matchesQuestDebugFilter(state, QuestDebugFilter::ReadyToTurnIn));
}

TEST(QuestDebugPanelHelpersTest, CompletedStateWinsOverOtherQuestStateChecks) {
    const auto quest = makeQuest("quest.debug.completed");
    game::component::QuestLogComponent quest_log{};
    quest_log.active_quests.push_back(quest.id_);
    quest_log.completed_quests.push_back(quest.id_);
    quest_log.objective_progress[game::data::makeQuestObjectiveProgressKey(quest.id_, quest.objectives_.front().id_)] =
        quest.objectives_.front().required_count_;

    EXPECT_EQ(detail::resolveQuestDebugState(quest_log, quest), QuestDebugState::Completed);
}

TEST(QuestDebugPanelHelpersTest, ClampQuestObjectiveProgressCapsAtRequiredCountAndTreatsMissingAsZero) {
    const auto quest = makeQuest("quest.debug.progress", "bat_hunt", 2);
    const auto& objective = quest.objectives_.front();
    game::component::QuestLogComponent quest_log{};

    EXPECT_EQ(detail::clampQuestObjectiveProgress(quest_log, quest, objective), 0);

    quest_log.objective_progress[game::data::makeQuestObjectiveProgressKey(quest.id_, objective.id_)] = 99;
    EXPECT_EQ(detail::clampQuestObjectiveProgress(quest_log, quest, objective), 2);
}

TEST(QuestDebugPanelHelpersTest, ResolveQuestDebugSelectionReturnsVisibleErrorForMissingQuestDefinition) {
    game::data::QuestCatalog catalog;

    const auto result = detail::resolveQuestDebugSelection(&catalog, "quest.debug.missing");

    EXPECT_FALSE(result.ok());
    EXPECT_EQ(result.quest, nullptr);
    EXPECT_NE(result.error.find("未找到任务定义"), std::string::npos);
}

} // namespace
} // namespace game::debug
// NOLINTEND
