// NOLINTBEGIN
#include <gtest/gtest.h>

#include "battle_catalog_fixture.h"
#include "game/battle/battle_log_formatter.h"
#include "game/data/item_catalog.h"
#include "game/data/rpg_catalog.h"

#include <vector>

namespace game::battle {
namespace {

[[nodiscard]] BattleSnapshot makeSnapshot() {
    return BattleSnapshot{
        .units = {
            BattleUnit{.id = 1, .name = "Hero", .side = BattleSide::Player},
            BattleUnit{.id = 2, .name = "Partner", .side = BattleSide::Player},
            BattleUnit{.id = 101, .name = "Slime", .side = BattleSide::Enemy}
        },
        .turn_order = {1, 2, 101},
        .current_actor_id = BattleUnitId{1},
        .round_index = 1,
        .outcome = BattleOutcome::Ongoing
    };
}

TEST(BattleLogFormatterTest, FormatsAttackDamageAndKo) {
    BattleActionResult result{};
    result.status = BattleActionStatus::Applied;
    result.action_type = BattleActionType::Attack;
    result.actor_id = 1;
    result.target_id = 101;
    result.damage = 24;
    result.target_defeated = true;
    result.snapshot = makeSnapshot();

    const std::vector<BattleLogLine> lines = formatBattleLogLines(result);

    ASSERT_EQ(lines.size(), 3U);
    EXPECT_EQ(lines[0], (BattleLogLine{.text = "Hero attacks Slime!", .tone = BattleLogTone::Normal}));
    EXPECT_EQ(lines[1], (BattleLogLine{.text = "Slime takes 24 damage.", .tone = BattleLogTone::Damage}));
    EXPECT_EQ(lines[2], (BattleLogLine{.text = "Slime was defeated!", .tone = BattleLogTone::System}));
}

TEST(BattleLogFormatterTest, FormatsSkillNamesMissCriticalAndStatesFromCatalog) {
    const auto fixture = testdata::createCatalogFixture("battle_log_formatter_skill_fixture");
    game::data::RpgCatalog rpg_catalog;
    ASSERT_TRUE(rpg_catalog.loadSkills(fixture.skills.string()));
    ASSERT_TRUE(rpg_catalog.loadStates(fixture.states.string()));

    BattleActionResult miss_result{};
    miss_result.status = BattleActionStatus::Applied;
    miss_result.action_type = BattleActionType::Skill;
    miss_result.actor_id = 1;
    miss_result.target_id = 101;
    miss_result.skill_id = "skill.fire";
    miss_result.missed = true;
    miss_result.snapshot = makeSnapshot();

    const std::vector<BattleLogLine> miss_lines =
        formatBattleLogLines(miss_result, BattleLogFormatterContext{.rpg_catalog = &rpg_catalog});
    ASSERT_EQ(miss_lines.size(), 2U);
    EXPECT_EQ(miss_lines[0].text, "Hero uses Fire!");
    EXPECT_EQ(miss_lines[1], (BattleLogLine{.text = "Slime evades the attack.", .tone = BattleLogTone::System}));

    BattleActionResult critical_state_result{};
    critical_state_result.status = BattleActionStatus::Applied;
    critical_state_result.action_type = BattleActionType::Skill;
    critical_state_result.actor_id = 1;
    critical_state_result.target_id = 101;
    critical_state_result.skill_id = "skill.multi_state";
    critical_state_result.damage = 15;
    critical_state_result.critical = true;
    critical_state_result.states_added = {"state.burn", "state.stun"};
    critical_state_result.snapshot = makeSnapshot();

    const std::vector<BattleLogLine> critical_lines =
        formatBattleLogLines(critical_state_result, BattleLogFormatterContext{.rpg_catalog = &rpg_catalog});
    ASSERT_EQ(critical_lines.size(), 3U);
    EXPECT_EQ(critical_lines[0].text, "Hero uses Multi State!");
    EXPECT_EQ(critical_lines[1].tone, BattleLogTone::Damage);
    EXPECT_EQ(critical_lines[1].text, "Critical hit! Slime takes 15 damage.");
    EXPECT_EQ(critical_lines[2], (BattleLogLine{.text = "Slime gains Burn, Stun.", .tone = BattleLogTone::State}));
}

TEST(BattleLogFormatterTest, SplitsDenseFeedbackIntoShortLinesAndKeepsTerminalResult) {
    const auto fixture = testdata::createCatalogFixture("battle_log_formatter_dense_fixture");
    game::data::RpgCatalog rpg_catalog;
    ASSERT_TRUE(rpg_catalog.loadStates(fixture.states.string()));

    BattleActionResult result{};
    result.status = BattleActionStatus::Applied;
    result.action_type = BattleActionType::Skill;
    result.actor_id = 1;
    result.target_id = 101;
    result.skill_id = "skill.unknown";
    result.damage = 15;
    result.hp_recovered = 4;
    result.mp_recovered = 3;
    result.critical = true;
    result.target_guarded = true;
    result.target_defeated = true;
    result.states_added = {"state.burn", "state.stun"};
    result.states_removed = {"state.poison"};
    result.snapshot = makeSnapshot();

    const std::vector<BattleLogLine> lines =
        formatBattleLogLines(result, BattleLogFormatterContext{.rpg_catalog = &rpg_catalog});

    ASSERT_EQ(lines.size(), 3U);
    EXPECT_EQ(lines[0], (BattleLogLine{.text = "Hero uses skill.unknown!", .tone = BattleLogTone::Normal}));
    EXPECT_EQ(lines[1], (BattleLogLine{.text = "Critical hit! Guarded. Slime takes 15 damage.", .tone = BattleLogTone::Damage}));
    EXPECT_EQ(lines[2], (BattleLogLine{.text = "Slime was defeated!", .tone = BattleLogTone::System}));
}

TEST(BattleLogFormatterTest, IgnoresCriticalFlagWithoutDamage) {
    BattleActionResult result{};
    result.status = BattleActionStatus::Applied;
    result.action_type = BattleActionType::Skill;
    result.actor_id = 1;
    result.target_id = 101;
    result.skill_id = "skill.focus";
    result.critical = true;
    result.states_added = {"state.focus"};
    result.snapshot = makeSnapshot();

    const std::vector<BattleLogLine> lines = formatBattleLogLines(result);

    ASSERT_EQ(lines.size(), 2U);
    EXPECT_EQ(lines[0], (BattleLogLine{.text = "Hero uses skill.focus!", .tone = BattleLogTone::Normal}));
    EXPECT_EQ(lines[1], (BattleLogLine{.text = "Slime gains state.focus.", .tone = BattleLogTone::State}));
}

TEST(BattleLogFormatterTest, FormatsItemRecoveryAndEscape) {
    const auto fixture = testdata::createCatalogFixture("battle_log_formatter_item_fixture");
    game::data::ItemCatalog item_catalog;
    ASSERT_TRUE(item_catalog.loadItemConfig(fixture.items.string()));

    BattleActionResult item_result{};
    item_result.status = BattleActionStatus::Applied;
    item_result.action_type = BattleActionType::Item;
    item_result.actor_id = 1;
    item_result.target_id = 2;
    item_result.item_id = "item.potion";
    item_result.hp_recovered = 50;
    item_result.snapshot = makeSnapshot();

    const std::vector<BattleLogLine> item_lines =
        formatBattleLogLines(item_result, BattleLogFormatterContext{.item_catalog = &item_catalog});
    ASSERT_EQ(item_lines.size(), 2U);
    EXPECT_EQ(item_lines[0].text, "Hero uses Potion!");
    EXPECT_EQ(item_lines[1], (BattleLogLine{.text = "Partner recovers 50 HP.", .tone = BattleLogTone::Recovery}));

    BattleActionResult escape_result{};
    escape_result.status = BattleActionStatus::Applied;
    escape_result.action_type = BattleActionType::Escape;
    escape_result.actor_id = 1;
    escape_result.escape_succeeded = false;
    escape_result.snapshot = makeSnapshot();

    const std::vector<BattleLogLine> escape_lines = formatBattleLogLines(escape_result);
    ASSERT_EQ(escape_lines.size(), 2U);
    EXPECT_EQ(escape_lines[0].text, "Hero tries to escape.");
    EXPECT_EQ(escape_lines[1], (BattleLogLine{.text = "Escape failed!", .tone = BattleLogTone::Error}));
}

TEST(BattleLogFormatterTest, FormatsAggregateSkillWithoutSpecificTarget) {
    const auto fixture = testdata::createCatalogFixture("battle_log_formatter_aggregate_fixture");
    game::data::RpgCatalog rpg_catalog;
    ASSERT_TRUE(rpg_catalog.loadSkills(fixture.skills.string()));

    BattleActionResult result{};
    result.status = BattleActionStatus::Applied;
    result.action_type = BattleActionType::Skill;
    result.actor_id = 1;
    result.skill_id = "skill.cleave";
    result.damage = 24;
    result.target_defeated = true;
    result.snapshot = makeSnapshot();

    const std::vector<BattleLogLine> lines =
        formatBattleLogLines(result, BattleLogFormatterContext{.rpg_catalog = &rpg_catalog});

    ASSERT_EQ(lines.size(), 3U);
    EXPECT_EQ(lines[0].text, "Hero uses Cleave!");
    EXPECT_EQ(lines[1], (BattleLogLine{.text = "Targets take 24 damage.", .tone = BattleLogTone::Damage}));
    EXPECT_EQ(lines[2], (BattleLogLine{.text = "A target was defeated!", .tone = BattleLogTone::System}));
}

TEST(BattleLogFormatterTest, FormatsSelfSkillWithoutSpecificTargetAsActor) {
    const auto fixture = testdata::createCatalogFixture("battle_log_formatter_self_fixture");
    game::data::RpgCatalog rpg_catalog;
    ASSERT_TRUE(rpg_catalog.loadSkills(fixture.skills.string()));

    BattleActionResult result{};
    result.status = BattleActionStatus::Applied;
    result.action_type = BattleActionType::Skill;
    result.actor_id = 1;
    result.skill_id = "skill.self_mend";
    result.hp_recovered = 15;
    result.snapshot = makeSnapshot();

    const std::vector<BattleLogLine> lines =
        formatBattleLogLines(result, BattleLogFormatterContext{.rpg_catalog = &rpg_catalog});

    ASSERT_EQ(lines.size(), 2U);
    EXPECT_EQ(lines[0].text, "Hero uses Self Mend!");
    EXPECT_EQ(lines[1], (BattleLogLine{.text = "Hero recovers 15 HP.", .tone = BattleLogTone::Recovery}));
}

TEST(BattleLogFormatterTest, FormatsRejectedAndFallbackNames) {
    BattleActionResult rejected{};
    rejected.status = BattleActionStatus::Rejected;
    rejected.action_type = BattleActionType::Skill;
    rejected.actor_id = 77;
    rejected.target_id = 88;
    rejected.skill_id = "skill.unknown";
    rejected.failure_reason = "skill does not exist";

    const std::vector<BattleLogLine> rejected_lines = formatBattleLogLines(rejected);
    ASSERT_EQ(rejected_lines.size(), 1U);
    EXPECT_EQ(rejected_lines[0], (BattleLogLine{.text = "Action failed: skill does not exist", .tone = BattleLogTone::Error}));

    rejected.status = BattleActionStatus::Applied;
    rejected.failure_reason.clear();
    rejected.missed = true;

    const std::vector<BattleLogLine> fallback_lines = formatBattleLogLines(rejected);
    ASSERT_EQ(fallback_lines.size(), 2U);
    EXPECT_EQ(fallback_lines[0].text, "Actor #77 uses skill.unknown!");
    EXPECT_EQ(fallback_lines[1].text, "Target #88 evades the attack.");
}

} // namespace
} // namespace game::battle
// NOLINTEND
