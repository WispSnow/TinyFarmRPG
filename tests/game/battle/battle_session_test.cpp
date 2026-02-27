// NOLINTBEGIN
#include <gtest/gtest.h>

#include "appearance_test_fixture_utils.h"
#include "game/battle/battle_session.h"
#include "game/data/item_catalog.h"
#include "game/data/rpg_catalog.h"

#include <array>
#include <filesystem>
#include <optional>
#include <vector>

namespace game::battle {
namespace {

struct CatalogFixturePaths {
    std::filesystem::path skills{};
    std::filesystem::path states{};
    std::filesystem::path items{};
};

[[nodiscard]] CatalogFixturePaths createCatalogFixture() {
    const auto temp_root = game::test::createUniqueTempDir("battle_session_fixture");
    const auto data_root = temp_root / "data";
    std::filesystem::create_directories(data_root);

    game::test::writeTextFile(
        data_root / "skills.json",
        R"json({
  "skills": [
    {
      "id": "skill.fire",
      "display_name": "Fire",
      "scope": "one_enemy",
      "hit_type": "certain",
      "success_rate": 100,
      "repeats": 1,
      "damage": { "type": "hp_damage", "formula": "a.mat * 3 - b.mdf", "variance": 0, "critical": false },
      "effects": [ { "type": "add_state", "target_id": "state.burn", "value1": 1.0 } ]
    }
  ]
})json");

    game::test::writeTextFile(
        data_root / "states.json",
        R"json({
  "states": [
    { "id": "state.burn", "display_name": "Burn", "priority": 50, "min_turns": 2, "max_turns": 2, "traits": [] }
  ]
})json");

    game::test::writeTextFile(
        data_root / "items.json",
        R"json({
  "items": [
    {
      "id": "item.potion",
      "display_name": "Potion",
      "category": "consumable",
      "icon_id": "consumable/potion",
      "on_use": {
        "consume": 1,
        "effects": [ { "type": "add_item", "id": "item.empty_bottle", "count": 1 } ]
      }
    },
    {
      "id": "item.empty_bottle",
      "display_name": "Empty Bottle",
      "category": "material",
      "icon_id": "material/empty_bottle"
    }
  ]
})json");

    return CatalogFixturePaths{
        .skills = data_root / "skills.json",
        .states = data_root / "states.json",
        .items = data_root / "items.json"};
}

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
    const CatalogFixturePaths fixture = createCatalogFixture();

    game::data::RpgCatalog rpg_catalog;
    ASSERT_TRUE(rpg_catalog.loadSkills(fixture.skills.string()));
    ASSERT_TRUE(rpg_catalog.loadStates(fixture.states.string()));

    game::data::ItemCatalog item_catalog;
    ASSERT_TRUE(item_catalog.loadItemConfig(fixture.items.string()));

    BattleSessionOptions options{};
    options.rpg_catalog = &rpg_catalog;
    options.item_catalog = &item_catalog;
    options.item_stocks.insert_or_assign(game::data::RpgCatalog::hashId("item.potion"), 1);
    BattleSession session(makeSessionUnits(), std::move(options));
    ASSERT_EQ(session.currentActorId(), std::optional<BattleUnitId>{1});

    const BattleActionResult skill_result = session.submitAction(BattleAction{
        .type = BattleActionType::Skill,
        .actor_id = 1,
        .target_id = 101,
        .skill_id = "skill.fire"
    });
    EXPECT_EQ(skill_result.status, BattleActionStatus::Applied);
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
    const BattleActionResult item_result = session.submitAction(BattleAction{
        .type = BattleActionType::Item,
        .actor_id = 1,
        .item_id = "item.potion"
    });
    EXPECT_EQ(item_result.status, BattleActionStatus::Applied);
    EXPECT_EQ(item_result.outcome_after, BattleOutcome::Ongoing);
    EXPECT_EQ(session.currentActorId(), std::optional<BattleUnitId>{2});
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
