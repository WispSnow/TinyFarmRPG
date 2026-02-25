// NOLINTBEGIN
#include <gtest/gtest.h>

#include "game/battle/battle_session.h"

#include <optional>
#include <vector>

namespace game::battle {
namespace {

std::vector<BattleUnit> makeSessionUnits() {
    return {
        BattleUnit{1, "Hero", BattleSide::Player, 120, 120, 30, 20},
        BattleUnit{2, "Partner", BattleSide::Player, 90, 90, 15, 15},
        BattleUnit{101, "Slime", BattleSide::Enemy, 40, 40, 10, 10},
        BattleUnit{102, "Bat", BattleSide::Enemy, 50, 50, 12, 8}
    };
}

TEST(BattleSessionTest, AttackActionAppliesDamageAndCanEndBattle) {
    BattleSession session(makeSessionUnits());
    ASSERT_EQ(session.currentActorId(), std::optional<BattleUnitId>{1});

    const BattleAction action{
        .type = BattleActionType::Attack,
        .actor_id = 1,
        .target_id = 101
    };

    const BattleActionResult result = session.submitAction(action);

    EXPECT_EQ(result.status, BattleActionStatus::Applied);
    EXPECT_EQ(result.damage, 30);
    EXPECT_FALSE(result.target_defeated);
    EXPECT_EQ(result.outcome_after, BattleOutcome::Ongoing);

    const auto* slime = session.findUnit(101);
    ASSERT_NE(slime, nullptr);
    EXPECT_EQ(slime->hp, 10);
    EXPECT_EQ(session.currentActorId(), std::optional<BattleUnitId>{2});
}

TEST(BattleSessionTest, EndTurnMovesToNextAliveActor) {
    BattleSession session(makeSessionUnits());
    ASSERT_EQ(session.currentActorId(), std::optional<BattleUnitId>{1});

    const BattleActionResult result = session.submitAction(BattleAction{
        .type = BattleActionType::EndTurn,
        .actor_id = 1,
        .target_id = std::nullopt
    });

    EXPECT_EQ(result.status, BattleActionStatus::Applied);
    EXPECT_EQ(result.outcome_after, BattleOutcome::Ongoing);
    EXPECT_EQ(session.currentActorId(), std::optional<BattleUnitId>{2});
}

TEST(BattleSessionTest, RejectsActionFromNonCurrentActor) {
    BattleSession session(makeSessionUnits());
    ASSERT_EQ(session.currentActorId(), std::optional<BattleUnitId>{1});

    const BattleActionResult result = session.submitAction(BattleAction{
        .type = BattleActionType::Attack,
        .actor_id = 2,
        .target_id = 101
    });

    EXPECT_EQ(result.status, BattleActionStatus::Rejected);
    EXPECT_EQ(result.outcome_after, BattleOutcome::Ongoing);
    EXPECT_EQ(session.currentActorId(), std::optional<BattleUnitId>{1});
}

TEST(BattleSessionTest, RejectsFriendlyTargetAttack) {
    BattleSession session(makeSessionUnits());

    const BattleActionResult result = session.submitAction(BattleAction{
        .type = BattleActionType::Attack,
        .actor_id = 1,
        .target_id = 2
    });

    EXPECT_EQ(result.status, BattleActionStatus::Rejected);
    EXPECT_EQ(session.currentActorId(), std::optional<BattleUnitId>{1});
}

} // namespace
} // namespace game::battle
// NOLINTEND
