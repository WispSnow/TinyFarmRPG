// NOLINTBEGIN
#include <gtest/gtest.h>

#include "game/battle/turn_core.h"

#include <optional>
#include <string>
#include <vector>

namespace game::battle {
namespace {

std::vector<BattleUnit> makeUnits() {
    return {
        BattleUnit{
            .id = 1,
            .name = "Hero",
            .side = BattleSide::Player,
            .hp = 120,
            .max_hp = 120,
            .mp = 0,
            .max_mp = 0,
            .attack = 20,
            .defense = 10,
            .magic_attack = 10,
            .magic_defense = 10,
            .speed = 12,
            .luck = 10},
        BattleUnit{
            .id = 2,
            .name = "Mage",
            .side = BattleSide::Player,
            .hp = 80,
            .max_hp = 80,
            .mp = 0,
            .max_mp = 0,
            .attack = 16,
            .defense = 10,
            .magic_attack = 10,
            .magic_defense = 10,
            .speed = 12,
            .luck = 10},
        BattleUnit{
            .id = 101,
            .name = "Slime",
            .side = BattleSide::Enemy,
            .hp = 60,
            .max_hp = 60,
            .mp = 0,
            .max_mp = 0,
            .attack = 10,
            .defense = 10,
            .magic_attack = 10,
            .magic_defense = 10,
            .speed = 18,
            .luck = 10},
        BattleUnit{
            .id = 102,
            .name = "Bat",
            .side = BattleSide::Enemy,
            .hp = 55,
            .max_hp = 55,
            .mp = 0,
            .max_mp = 0,
            .attack = 12,
            .defense = 10,
            .magic_attack = 10,
            .magic_defense = 10,
            .speed = 8,
            .luck = 10}
    };
}

TEST(TurnCoreTest, SortsBySpeedAndKeepsStableTieOrder) {
    TurnCore turn_core(makeUnits());

    const std::vector<BattleUnitId> expected_order{101, 1, 2, 102};
    EXPECT_EQ(turn_core.turnOrder(), expected_order);

    ASSERT_EQ(turn_core.currentActorId(), std::optional<BattleUnitId>{101});
    EXPECT_TRUE(turn_core.advanceTurn());
    EXPECT_EQ(turn_core.currentActorId(), std::optional<BattleUnitId>{1});
    EXPECT_TRUE(turn_core.advanceTurn());
    EXPECT_EQ(turn_core.currentActorId(), std::optional<BattleUnitId>{2});
}

TEST(TurnCoreTest, SkipsDefeatedUnitsWhenAdvancing) {
    auto units = makeUnits();
    units[1].hp = 0; // Mage defeated before entering battle loop

    TurnCore turn_core(std::move(units));
    ASSERT_EQ(turn_core.currentActorId(), std::optional<BattleUnitId>{101});

    EXPECT_TRUE(turn_core.advanceTurn());
    EXPECT_EQ(turn_core.currentActorId(), std::optional<BattleUnitId>{1});

    auto* hero = turn_core.findUnitMutable(1);
    ASSERT_NE(hero, nullptr);
    hero->hp = 0;
    turn_core.refresh();

    EXPECT_EQ(turn_core.outcome(), BattleOutcome::Defeat);
    EXPECT_FALSE(turn_core.currentActorId().has_value());
}

TEST(TurnCoreTest, EvaluatesOutcomeFromAliveSides) {
    TurnCore turn_core(makeUnits());
    EXPECT_EQ(turn_core.outcome(), BattleOutcome::Ongoing);

    auto* slime = turn_core.findUnitMutable(101);
    auto* bat = turn_core.findUnitMutable(102);
    ASSERT_NE(slime, nullptr);
    ASSERT_NE(bat, nullptr);

    slime->hp = 0;
    bat->hp = 0;
    turn_core.refresh();

    EXPECT_EQ(turn_core.outcome(), BattleOutcome::Victory);
    EXPECT_FALSE(turn_core.currentActorId().has_value());
}

TEST(TurnCoreTest, TracksRoundIndexWhenTurnOrderWraps) {
    TurnCore turn_core(makeUnits());
    ASSERT_EQ(turn_core.currentActorId(), std::optional<BattleUnitId>{101});
    EXPECT_EQ(turn_core.roundIndex(), 1U);

    EXPECT_TRUE(turn_core.advanceTurn());
    EXPECT_EQ(turn_core.currentActorId(), std::optional<BattleUnitId>{1});
    EXPECT_EQ(turn_core.roundIndex(), 1U);

    EXPECT_TRUE(turn_core.advanceTurn());
    EXPECT_EQ(turn_core.currentActorId(), std::optional<BattleUnitId>{2});
    EXPECT_EQ(turn_core.roundIndex(), 1U);

    EXPECT_TRUE(turn_core.advanceTurn());
    EXPECT_EQ(turn_core.currentActorId(), std::optional<BattleUnitId>{102});
    EXPECT_EQ(turn_core.roundIndex(), 1U);

    EXPECT_TRUE(turn_core.advanceTurn());
    EXPECT_EQ(turn_core.currentActorId(), std::optional<BattleUnitId>{101});
    EXPECT_EQ(turn_core.roundIndex(), 2U);
}

TEST(TurnCoreTest, EmitsRoundHooksOnWrap) {
    TurnCore turn_core(makeUnits());

    std::vector<std::string> events{};
    turn_core.setRoundHooks(
        [&events](const std::uint32_t round_index) {
            events.push_back("B" + std::to_string(round_index));
        },
        [&events](const std::uint32_t round_index) {
            events.push_back("E" + std::to_string(round_index));
        });

    EXPECT_TRUE(turn_core.advanceTurn());
    EXPECT_TRUE(turn_core.advanceTurn());
    EXPECT_TRUE(turn_core.advanceTurn());
    EXPECT_TRUE(turn_core.advanceTurn());

    const std::vector<std::string> expected{"E1", "B2"};
    EXPECT_EQ(events, expected);
}

TEST(TurnCoreTest, ForcedEscapeOutcomeSurvivesRefreshAndAdvance) {
    TurnCore turn_core(makeUnits());
    ASSERT_EQ(turn_core.outcome(), BattleOutcome::Ongoing);
    ASSERT_TRUE(turn_core.currentActorId().has_value());

    turn_core.forceOutcome(BattleOutcome::Escaped);
    turn_core.refresh();

    EXPECT_EQ(turn_core.outcome(), BattleOutcome::Escaped);
    EXPECT_FALSE(turn_core.currentActorId().has_value());
    EXPECT_FALSE(turn_core.advanceTurn());
    EXPECT_EQ(turn_core.outcome(), BattleOutcome::Escaped);
}

} // namespace
} // namespace game::battle
// NOLINTEND
