// NOLINTBEGIN
#include <gtest/gtest.h>

#include "game/battle/battle_action_resolver.h"
#include "game/battle/turn_core.h"

#include <optional>
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
            .attack = 30,
            .defense = 10,
            .magic_attack = 10,
            .magic_defense = 10,
            .speed = 20,
            .luck = 10},
        BattleUnit{
            .id = 2,
            .name = "Partner",
            .side = BattleSide::Player,
            .hp = 90,
            .max_hp = 90,
            .mp = 0,
            .max_mp = 0,
            .attack = 15,
            .defense = 10,
            .magic_attack = 10,
            .magic_defense = 10,
            .speed = 15,
            .luck = 10},
        BattleUnit{
            .id = 101,
            .name = "Slime",
            .side = BattleSide::Enemy,
            .hp = 40,
            .max_hp = 40,
            .mp = 0,
            .max_mp = 0,
            .attack = 10,
            .defense = 10,
            .magic_attack = 10,
            .magic_defense = 10,
            .speed = 10,
            .luck = 10}
    };
}

TEST(BattleActionResolverTest, AttackAppliesDamageAndAdvancesTurn) {
    TurnCore turn_core(makeUnits());
    ASSERT_EQ(turn_core.currentActorId(), std::optional<BattleUnitId>{1});

    BattleActionResolver resolver{};
    const BattleActionResult result = resolver.resolve(BattleAction{
        .type = BattleActionType::Attack,
        .actor_id = 1,
        .target_id = 101
    }, turn_core);

    EXPECT_EQ(result.status, BattleActionStatus::Applied);
    EXPECT_EQ(result.damage, 30);
    EXPECT_FALSE(result.target_defeated);
    EXPECT_EQ(turn_core.currentActorId(), std::optional<BattleUnitId>{2});

    const auto* slime = turn_core.findUnit(101);
    ASSERT_NE(slime, nullptr);
    EXPECT_EQ(slime->hp, 10);
}

TEST(BattleActionResolverTest, EndTurnAdvancesToNextActor) {
    TurnCore turn_core(makeUnits());
    ASSERT_EQ(turn_core.currentActorId(), std::optional<BattleUnitId>{1});

    BattleActionResolver resolver{};
    const BattleActionResult result = resolver.resolve(BattleAction{
        .type = BattleActionType::EndTurn,
        .actor_id = 1
    }, turn_core);

    EXPECT_EQ(result.status, BattleActionStatus::Applied);
    EXPECT_EQ(turn_core.currentActorId(), std::optional<BattleUnitId>{2});
}

TEST(BattleActionResolverTest, RejectsAttackWithoutValidTarget) {
    TurnCore turn_core(makeUnits());

    BattleActionResolver resolver{};
    const BattleActionResult result = resolver.resolve(BattleAction{
        .type = BattleActionType::Attack,
        .actor_id = 1,
        .target_id = 2
    }, turn_core);

    EXPECT_EQ(result.status, BattleActionStatus::Rejected);
    EXPECT_EQ(turn_core.currentActorId(), std::optional<BattleUnitId>{1});
}

TEST(BattleActionResolverTest, RejectsUnimplementedActionTypes) {
    TurnCore turn_core(makeUnits());
    BattleActionResolver resolver{};

    const BattleActionResult result = resolver.resolve(BattleAction{
        .type = BattleActionType::Skill,
        .actor_id = 1,
        .target_id = 101,
        .skill_id = "skill.stub"
    }, turn_core);

    EXPECT_EQ(result.status, BattleActionStatus::Rejected);
    EXPECT_EQ(turn_core.currentActorId(), std::optional<BattleUnitId>{1});
}

} // namespace
} // namespace game::battle
// NOLINTEND
