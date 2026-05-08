// NOLINTBEGIN
#include <gtest/gtest.h>

#include "battle_catalog_fixture.h"
#include "game/battle/battle_session.h"
#include "game/data/item_catalog.h"
#include "game/data/rpg_catalog.h"

#include <array>
#include <algorithm>
#include <optional>
#include <utility>
#include <vector>

namespace game::battle {
namespace {

std::vector<BattleUnit> makeSessionUnits() {
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
            .luck = 10},
        BattleUnit{
            .id = 102,
            .name = "Bat",
            .side = BattleSide::Enemy,
            .hp = 50,
            .max_hp = 50,
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

TEST(BattleSessionTest, SnapshotIncludesStableTurnOrder) {
    BattleSession session(makeSessionUnits());

    const BattleSnapshot snapshot = session.snapshot();

    const std::vector<BattleUnitId> expected_order{1, 2, 101, 102};
    EXPECT_EQ(snapshot.turn_order, expected_order);
    EXPECT_EQ(snapshot.current_actor_id, std::optional<BattleUnitId>{1});
}

TEST(BattleSessionTest, ExposesRoundIndexForPresentationLayerGates) {
    BattleSession session(makeSessionUnits());
    EXPECT_EQ(session.roundIndex(), 1U);

    EXPECT_EQ(session.submitAction(BattleAction{
                  .type = BattleActionType::EndTurn,
                  .actor_id = 1
              }).status,
              BattleActionStatus::Applied);
    EXPECT_EQ(session.roundIndex(), 1U);

    EXPECT_EQ(session.submitAction(BattleAction{
                  .type = BattleActionType::EndTurn,
                  .actor_id = 2
              }).status,
              BattleActionStatus::Applied);
    EXPECT_EQ(session.submitAction(BattleAction{
                  .type = BattleActionType::EndTurn,
                  .actor_id = 101
              }).status,
              BattleActionStatus::Applied);
    EXPECT_EQ(session.submitAction(BattleAction{
                  .type = BattleActionType::EndTurn,
                  .actor_id = 102
              }).status,
              BattleActionStatus::Applied);

    EXPECT_EQ(session.currentActorId(), std::optional<BattleUnitId>{1});
    EXPECT_EQ(session.roundIndex(), 2U);
}

TEST(BattleSessionTest, SnapshotTurnOrderRemainsStableWhenDefeatedActorIsSkipped) {
    auto units = makeSessionUnits();
    units[0].attack = 90;
    BattleSession session(std::move(units));
    ASSERT_EQ(session.currentActorId(), std::optional<BattleUnitId>{1});

    const BattleActionResult result = session.submitAction(BattleAction{
        .type = BattleActionType::Attack,
        .actor_id = 1,
        .target_id = 101
    });

    const std::vector<BattleUnitId> expected_order{1, 2, 101, 102};
    EXPECT_EQ(result.status, BattleActionStatus::Applied);
    EXPECT_TRUE(result.target_defeated);
    EXPECT_EQ(result.snapshot.turn_order, expected_order);
    EXPECT_EQ(result.snapshot.current_actor_id, std::optional<BattleUnitId>{2});

    const auto current_position =
        std::find(result.snapshot.turn_order.begin(), result.snapshot.turn_order.end(), *result.snapshot.current_actor_id);
    ASSERT_NE(current_position, result.snapshot.turn_order.end());
    EXPECT_EQ(static_cast<std::size_t>(std::distance(result.snapshot.turn_order.begin(), current_position)), 1U);

    const auto defeated_position = std::find(result.snapshot.turn_order.begin(), result.snapshot.turn_order.end(), BattleUnitId{101});
    ASSERT_NE(defeated_position, result.snapshot.turn_order.end());
    EXPECT_EQ(static_cast<std::size_t>(std::distance(result.snapshot.turn_order.begin(), defeated_position)), 2U);
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

TEST(BattleSessionTest, RejectsSkillAndItemWithoutCatalogsAndDoesNotAdvanceTurn) {
    BattleSession session(makeSessionUnits());
    ASSERT_EQ(session.currentActorId(), std::optional<BattleUnitId>{1});

    constexpr std::array<BattleActionType, 2> kCatalogRequiredTypes{
        BattleActionType::Skill,
        BattleActionType::Item};

    for (const auto action_type : kCatalogRequiredTypes) {
        const BattleSnapshot before = session.snapshot();
        const BattleActionResult result = session.submitAction(BattleAction{
            .type = action_type,
            .actor_id = 1,
            .target_id = 101,
            .skill_id = "skill.stub",
            .item_id = "item.stub"
        });

        EXPECT_EQ(result.status, BattleActionStatus::Rejected);
        EXPECT_EQ(result.action_type, action_type);
        EXPECT_EQ(result.outcome_after, BattleOutcome::Ongoing);
        EXPECT_NE(result.failure_reason.find("catalog"), std::string::npos);
        EXPECT_EQ(result.snapshot.current_actor_id, before.current_actor_id);
        EXPECT_EQ(session.currentActorId(), std::optional<BattleUnitId>{1});
    }

    const BattleUnit* target = session.findUnit(101);
    ASSERT_NE(target, nullptr);
    EXPECT_EQ(target->hp, 40);
}

TEST(BattleSessionTest, SkillAndItemActionsApplyWhenCatalogsAreProvided) {
    const auto fixture = testdata::createCatalogFixture("battle_session_fixture");

    game::data::RpgCatalog rpg_catalog;
    ASSERT_TRUE(rpg_catalog.loadSkills(fixture.skills.string()));
    ASSERT_TRUE(rpg_catalog.loadStates(fixture.states.string()));

    game::data::ItemCatalog item_catalog;
    ASSERT_TRUE(item_catalog.loadItemConfig(fixture.items.string()));

    BattleSessionOptions options{};
    options.rpg_catalog = &rpg_catalog;
    options.item_catalog = &item_catalog;
    options.item_stocks.insert_or_assign(game::data::RpgCatalog::hashId("item.potion"), 1);
    auto units = makeSessionUnits();
    units[1].hp = 40;
    BattleSession session(std::move(units), std::move(options));
    ASSERT_EQ(session.currentActorId(), std::optional<BattleUnitId>{1});

    const BattleActionResult skill_result = session.submitAction(BattleAction{
        .type = BattleActionType::Skill,
        .actor_id = 1,
        .target_id = 101,
        .skill_id = "skill.fire"
    });
    EXPECT_EQ(skill_result.status, BattleActionStatus::Applied);
    EXPECT_EQ(skill_result.skill_id, "skill.fire");
    EXPECT_EQ(skill_result.damage, 20);
    EXPECT_EQ(session.currentActorId(), std::optional<BattleUnitId>{2});

    EXPECT_EQ(session.submitAction(BattleAction{
                  .type = BattleActionType::EndTurn,
                  .actor_id = 2
              }).status,
              BattleActionStatus::Applied);
    EXPECT_EQ(session.submitAction(BattleAction{
                  .type = BattleActionType::EndTurn,
                  .actor_id = 101
              }).status,
              BattleActionStatus::Applied);
    EXPECT_EQ(session.submitAction(BattleAction{
                  .type = BattleActionType::EndTurn,
                  .actor_id = 102
              }).status,
              BattleActionStatus::Applied);

    ASSERT_EQ(session.currentActorId(), std::optional<BattleUnitId>{1});
    ASSERT_NE(session.findUnit(1), nullptr);
    const BattleActionResult item_result = session.submitAction(BattleAction{
        .type = BattleActionType::Item,
        .actor_id = 1,
        .target_id = 2,
        .item_id = "item.potion"
    });
    EXPECT_EQ(item_result.status, BattleActionStatus::Applied);
    EXPECT_EQ(item_result.item_id, "item.potion");
    EXPECT_EQ(item_result.hp_recovered, 50);
    EXPECT_EQ(item_result.outcome_after, BattleOutcome::Ongoing);
    ASSERT_NE(session.findUnit(2), nullptr);
    EXPECT_EQ(session.findUnit(2)->hp, 90);
    EXPECT_EQ(session.itemStocks().count(game::data::RpgCatalog::hashId("item.potion")), 0U);
    EXPECT_EQ(session.currentActorId(), std::optional<BattleUnitId>{2});
}

TEST(BattleSessionTest, SnapshotIncludesActiveStatesSortedByPriority) {
    const auto fixture = testdata::createCatalogFixture("battle_session_state_snapshot_fixture");

    game::data::RpgCatalog rpg_catalog;
    ASSERT_TRUE(rpg_catalog.loadSkills(fixture.skills.string()));
    ASSERT_TRUE(rpg_catalog.loadStates(fixture.states.string()));

    BattleSessionOptions options{};
    options.rpg_catalog = &rpg_catalog;
    BattleSession session(makeSessionUnits(), std::move(options));

    const BattleActionResult result = session.submitAction(BattleAction{
        .type = BattleActionType::Skill,
        .actor_id = 1,
        .target_id = 101,
        .skill_id = "skill.multi_state"
    });
    ASSERT_EQ(result.status, BattleActionStatus::Applied);

    const BattleSnapshot& snapshot = result.snapshot;
    ASSERT_EQ(snapshot.unit_states.size(), 1U);
    EXPECT_EQ(snapshot.unit_states[0].unit_id, BattleUnitId{101});
    ASSERT_EQ(snapshot.unit_states[0].states.size(), 3U);
    EXPECT_EQ(snapshot.unit_states[0].states[0].state_id, "state.stun");
    EXPECT_EQ(snapshot.unit_states[0].states[0].turns_left, 1);
    EXPECT_EQ(snapshot.unit_states[0].states[1].state_id, "state.bleed");
    EXPECT_EQ(snapshot.unit_states[0].states[1].turns_left, 3);
    EXPECT_EQ(snapshot.unit_states[0].states[2].state_id, "state.burn");
    EXPECT_EQ(snapshot.unit_states[0].states[2].turns_left, 2);
}

TEST(BattleSessionTest, SnapshotActiveStatesDecrementAndExpireAcrossRounds) {
    const auto fixture = testdata::createCatalogFixture("battle_session_state_expire_fixture");

    game::data::RpgCatalog rpg_catalog;
    ASSERT_TRUE(rpg_catalog.loadSkills(fixture.skills.string()));
    ASSERT_TRUE(rpg_catalog.loadStates(fixture.states.string()));

    BattleSessionOptions options{};
    options.rpg_catalog = &rpg_catalog;
    BattleSession session(makeSessionUnits(), std::move(options));

    ASSERT_EQ(session.submitAction(BattleAction{
                  .type = BattleActionType::Skill,
                  .actor_id = 1,
                  .target_id = 101,
                  .skill_id = "skill.multi_state"
              }).status,
              BattleActionStatus::Applied);
    ASSERT_EQ(session.submitAction(BattleAction{
                  .type = BattleActionType::EndTurn,
                  .actor_id = 2
              }).status,
              BattleActionStatus::Applied);
    ASSERT_EQ(session.submitAction(BattleAction{
                  .type = BattleActionType::EndTurn,
                  .actor_id = 101
              }).status,
              BattleActionStatus::Applied);
    const BattleActionResult round_end = session.submitAction(BattleAction{
        .type = BattleActionType::EndTurn,
        .actor_id = 102
    });
    ASSERT_EQ(round_end.status, BattleActionStatus::Applied);

    ASSERT_EQ(round_end.snapshot.unit_states.size(), 1U);
    EXPECT_EQ(round_end.snapshot.unit_states[0].unit_id, BattleUnitId{101});
    ASSERT_EQ(round_end.snapshot.unit_states[0].states.size(), 2U);
    EXPECT_EQ(round_end.snapshot.unit_states[0].states[0].state_id, "state.bleed");
    EXPECT_EQ(round_end.snapshot.unit_states[0].states[0].turns_left, 2);
    EXPECT_EQ(round_end.snapshot.unit_states[0].states[1].state_id, "state.burn");
    EXPECT_EQ(round_end.snapshot.unit_states[0].states[1].turns_left, 1);
}

TEST(BattleSessionTest, SnapshotSkipsStatesForDefeatedUnits) {
    const auto fixture = testdata::createCatalogFixture("battle_session_ko_state_snapshot_fixture");

    game::data::RpgCatalog rpg_catalog;
    ASSERT_TRUE(rpg_catalog.loadSkills(fixture.skills.string()));
    ASSERT_TRUE(rpg_catalog.loadStates(fixture.states.string()));

    BattleSessionOptions options{};
    options.rpg_catalog = &rpg_catalog;
    auto units = makeSessionUnits();
    units[1].attack = 90;
    BattleSession session(std::move(units), std::move(options));

    ASSERT_EQ(session.submitAction(BattleAction{
                  .type = BattleActionType::Skill,
                  .actor_id = 1,
                  .target_id = 101,
                  .skill_id = "skill.multi_state"
              }).status,
              BattleActionStatus::Applied);

    const BattleActionResult defeat_result = session.submitAction(BattleAction{
        .type = BattleActionType::Attack,
        .actor_id = 2,
        .target_id = 101
    });
    EXPECT_EQ(defeat_result.status, BattleActionStatus::Applied);
    EXPECT_TRUE(defeat_result.target_defeated);
    EXPECT_TRUE(defeat_result.snapshot.unit_states.empty());
}

TEST(BattleSessionTest, GuardActionCanReduceIncomingDamage) {
    BattleSession session(makeSessionUnits());
    ASSERT_EQ(session.currentActorId(), std::optional<BattleUnitId>{1});

    const BattleActionResult guard_result = session.submitAction(BattleAction{
        .type = BattleActionType::Guard,
        .actor_id = 1
    });
    EXPECT_EQ(guard_result.status, BattleActionStatus::Applied);
    ASSERT_EQ(session.currentActorId(), std::optional<BattleUnitId>{2});

    const BattleActionResult partner_end_turn = session.submitAction(BattleAction{
        .type = BattleActionType::EndTurn,
        .actor_id = 2
    });
    EXPECT_EQ(partner_end_turn.status, BattleActionStatus::Applied);
    ASSERT_EQ(session.currentActorId(), std::optional<BattleUnitId>{101});

    const BattleActionResult enemy_attack = session.submitAction(BattleAction{
        .type = BattleActionType::Attack,
        .actor_id = 101,
        .target_id = 1
    });
    EXPECT_EQ(enemy_attack.status, BattleActionStatus::Applied);
    EXPECT_EQ(enemy_attack.damage, 5);
    EXPECT_TRUE(enemy_attack.target_guarded);
}

TEST(BattleSessionTest, GuardStateClearsAtNextRoundBegin) {
    BattleSession session(makeSessionUnits());
    ASSERT_EQ(session.currentActorId(), std::optional<BattleUnitId>{1});

    const BattleActionResult guard_result = session.submitAction(BattleAction{
        .type = BattleActionType::Guard,
        .actor_id = 1
    });
    EXPECT_EQ(guard_result.status, BattleActionStatus::Applied);
    ASSERT_EQ(session.currentActorId(), std::optional<BattleUnitId>{2});

    EXPECT_EQ(session.submitAction(BattleAction{
                  .type = BattleActionType::EndTurn,
                  .actor_id = 2
              }).status,
              BattleActionStatus::Applied);
    EXPECT_EQ(session.submitAction(BattleAction{
                  .type = BattleActionType::EndTurn,
                  .actor_id = 101
              }).status,
              BattleActionStatus::Applied);
    EXPECT_EQ(session.submitAction(BattleAction{
                  .type = BattleActionType::EndTurn,
                  .actor_id = 102
              }).status,
              BattleActionStatus::Applied);

    ASSERT_EQ(session.currentActorId(), std::optional<BattleUnitId>{1});

    EXPECT_EQ(session.submitAction(BattleAction{
                  .type = BattleActionType::EndTurn,
                  .actor_id = 1
              }).status,
              BattleActionStatus::Applied);
    EXPECT_EQ(session.submitAction(BattleAction{
                  .type = BattleActionType::EndTurn,
                  .actor_id = 2
              }).status,
              BattleActionStatus::Applied);
    ASSERT_EQ(session.currentActorId(), std::optional<BattleUnitId>{101});

    const BattleActionResult enemy_attack = session.submitAction(BattleAction{
        .type = BattleActionType::Attack,
        .actor_id = 101,
        .target_id = 1
    });
    EXPECT_EQ(enemy_attack.status, BattleActionStatus::Applied);
    EXPECT_EQ(enemy_attack.damage, 10);
    EXPECT_FALSE(enemy_attack.target_guarded);
}

} // namespace
} // namespace game::battle
// NOLINTEND
