// NOLINTBEGIN
#include <gtest/gtest.h>

#include "game/battle/turn_core.h"

#include <optional>
#include <vector>

namespace game::battle {
namespace {

std::vector<BattleUnit> makeUnits() {
    return {
        BattleUnit{1, "Hero", BattleSide::Player, 120, 120, 20, 12},
        BattleUnit{2, "Mage", BattleSide::Player, 80, 80, 16, 12},
        BattleUnit{101, "Slime", BattleSide::Enemy, 60, 60, 10, 18},
        BattleUnit{102, "Bat", BattleSide::Enemy, 55, 55, 12, 8}
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

} // namespace
} // namespace game::battle
// NOLINTEND
