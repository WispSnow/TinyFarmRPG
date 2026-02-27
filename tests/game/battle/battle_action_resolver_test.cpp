// NOLINTBEGIN
#include <gtest/gtest.h>

#include "appearance_test_fixture_utils.h"
#include "game/battle/battle_action_resolver.h"
#include "game/battle/turn_core.h"
#include "game/data/item_catalog.h"
#include "game/data/rpg_catalog.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace game::battle {
namespace {

struct CatalogFixturePaths {
    std::filesystem::path skills{};
    std::filesystem::path states{};
    std::filesystem::path items{};
};

[[nodiscard]] CatalogFixturePaths createCatalogFixture() {
    const auto temp_root = game::test::createUniqueTempDir("battle_action_resolver_fixture");
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
      "damage": {
        "type": "hp_damage",
        "formula": "a.mat * 3 - b.mdf",
        "variance": 0,
        "critical": false
      },
      "effects": [
        { "type": "add_state", "target_id": "state.burn", "value1": 1.0 }
      ]
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
        "effects": [
          { "type": "add_item", "id": "item.empty_bottle", "count": 2 }
        ]
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

BattleRuntimeState makeRuntimeState(const TurnCore& turn_core) {
    BattleRuntimeState runtime_state{};
    for (const auto& unit : turn_core.units()) {
        runtime_state.units.try_emplace(unit.id);
    }
    return runtime_state;
}

TEST(BattleActionResolverTest, AttackAppliesDamageAndAdvancesTurn) {
    TurnCore turn_core(makeUnits());
    ASSERT_EQ(turn_core.currentActorId(), std::optional<BattleUnitId>{1});
    BattleRuntimeState runtime_state = makeRuntimeState(turn_core);

    BattleActionResolver resolver{};
    const BattleActionResult result = resolver.resolve(BattleAction{
        .type = BattleActionType::Attack,
        .actor_id = 1,
        .target_id = 101
    }, turn_core, runtime_state);

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
    BattleRuntimeState runtime_state = makeRuntimeState(turn_core);

    BattleActionResolver resolver{};
    const BattleActionResult result = resolver.resolve(BattleAction{
        .type = BattleActionType::EndTurn,
        .actor_id = 1
    }, turn_core, runtime_state);

    EXPECT_EQ(result.status, BattleActionStatus::Applied);
    EXPECT_EQ(turn_core.currentActorId(), std::optional<BattleUnitId>{2});
}

TEST(BattleActionResolverTest, RejectsAttackWithoutValidTarget) {
    TurnCore turn_core(makeUnits());
    BattleRuntimeState runtime_state = makeRuntimeState(turn_core);

    BattleActionResolver resolver{};
    const BattleActionResult result = resolver.resolve(BattleAction{
        .type = BattleActionType::Attack,
        .actor_id = 1,
        .target_id = 2
    }, turn_core, runtime_state);

    EXPECT_EQ(result.status, BattleActionStatus::Rejected);
    EXPECT_FALSE(result.failure_reason.empty());
    EXPECT_EQ(turn_core.currentActorId(), std::optional<BattleUnitId>{1});
}

TEST(BattleActionResolverTest, RejectsSkillAndItemWhenCatalogUnavailable) {
    TurnCore turn_core(makeUnits());
    BattleRuntimeState runtime_state = makeRuntimeState(turn_core);
    BattleActionResolver resolver{};

    const BattleActionResult skill_result = resolver.resolve(BattleAction{
        .type = BattleActionType::Skill,
        .actor_id = 1,
        .target_id = 101,
        .skill_id = "skill.stub"
    }, turn_core, runtime_state);

    EXPECT_EQ(skill_result.status, BattleActionStatus::Rejected);
    EXPECT_NE(skill_result.failure_reason.find("catalog"), std::string::npos);
    EXPECT_EQ(turn_core.currentActorId(), std::optional<BattleUnitId>{1});

    const BattleActionResult item_result = resolver.resolve(BattleAction{
        .type = BattleActionType::Item,
        .actor_id = 1,
        .target_id = 1,
        .item_id = "item.stub"
    }, turn_core, runtime_state);
    EXPECT_EQ(item_result.status, BattleActionStatus::Rejected);
    EXPECT_NE(item_result.failure_reason.find("catalog"), std::string::npos);
}

TEST(BattleActionResolverTest, SkillAppliesDamageAndAddsStateFromCatalog) {
    const CatalogFixturePaths fixture = createCatalogFixture();

    game::data::RpgCatalog rpg_catalog;
    ASSERT_TRUE(rpg_catalog.loadSkills(fixture.skills.string()));
    ASSERT_TRUE(rpg_catalog.loadStates(fixture.states.string()));

    TurnCore turn_core(makeUnits());
    BattleRuntimeState runtime_state = makeRuntimeState(turn_core);
    BattleActionResolver resolver{
        BattleActionResolver::Dependencies{
            .rpg_catalog = &rpg_catalog,
            .item_catalog = nullptr}};

    const BattleActionResult result = resolver.resolve(BattleAction{
        .type = BattleActionType::Skill,
        .actor_id = 1,
        .target_id = 101,
        .skill_id = "skill.fire"
    }, turn_core, runtime_state);

    EXPECT_EQ(result.status, BattleActionStatus::Applied);
    EXPECT_FALSE(result.missed);
    EXPECT_EQ(result.damage, 20);
    EXPECT_EQ(turn_core.currentActorId(), std::optional<BattleUnitId>{2});

    const auto* slime = turn_core.findUnit(101);
    ASSERT_NE(slime, nullptr);
    EXPECT_EQ(slime->hp, 20);
    EXPECT_EQ(runtime_state.units[101].state_turns_left["state.burn"], 2);
    EXPECT_EQ(result.states_added.size(), 1U);
    EXPECT_EQ(result.states_added[0], "state.burn");
}

TEST(BattleActionResolverTest, ItemConsumesStockAndTriggersOnUseEffects) {
    const CatalogFixturePaths fixture = createCatalogFixture();

    game::data::ItemCatalog item_catalog;
    ASSERT_TRUE(item_catalog.loadItemConfig(fixture.items.string()));

    TurnCore turn_core(makeUnits());
    BattleRuntimeState runtime_state = makeRuntimeState(turn_core);
    runtime_state.item_stocks[game::data::RpgCatalog::hashId("item.potion")] = 1;

    BattleActionResolver resolver{
        BattleActionResolver::Dependencies{
            .rpg_catalog = nullptr,
            .item_catalog = &item_catalog}};

    const BattleActionResult result = resolver.resolve(BattleAction{
        .type = BattleActionType::Item,
        .actor_id = 1,
        .item_id = "item.potion"
    }, turn_core, runtime_state);

    EXPECT_EQ(result.status, BattleActionStatus::Applied);
    EXPECT_EQ(turn_core.currentActorId(), std::optional<BattleUnitId>{2});
    EXPECT_EQ(runtime_state.item_stocks[game::data::RpgCatalog::hashId("item.empty_bottle")], 2);
    EXPECT_EQ(runtime_state.item_stocks.count(game::data::RpgCatalog::hashId("item.potion")), 0U);
}

TEST(BattleActionResolverTest, GuardReducesIncomingDamage) {
    TurnCore turn_core(makeUnits());
    BattleRuntimeState runtime_state = makeRuntimeState(turn_core);
    BattleActionResolver resolver{};

    const BattleActionResult guard_result = resolver.resolve(BattleAction{
        .type = BattleActionType::Guard,
        .actor_id = 1
    }, turn_core, runtime_state);
    EXPECT_EQ(guard_result.status, BattleActionStatus::Applied);

    const BattleActionResult partner_end_turn = resolver.resolve(BattleAction{
        .type = BattleActionType::EndTurn,
        .actor_id = 2
    }, turn_core, runtime_state);
    EXPECT_EQ(partner_end_turn.status, BattleActionStatus::Applied);
    ASSERT_EQ(turn_core.currentActorId(), std::optional<BattleUnitId>{101});

    const BattleActionResult enemy_attack = resolver.resolve(BattleAction{
        .type = BattleActionType::Attack,
        .actor_id = 101,
        .target_id = 1
    }, turn_core, runtime_state);

    EXPECT_EQ(enemy_attack.status, BattleActionStatus::Applied);
    EXPECT_EQ(enemy_attack.damage, 5);
    EXPECT_TRUE(enemy_attack.target_guarded);
}

TEST(BattleActionResolverTest, EscapeFailureAdvancesTurn) {
    TurnCore turn_core(makeUnits());
    BattleRuntimeState runtime_state = makeRuntimeState(turn_core);
    BattleActionResolver resolver{[] { return 80; }};

    const BattleActionResult result = resolver.resolve(BattleAction{
        .type = BattleActionType::Escape,
        .actor_id = 1
    }, turn_core, runtime_state);

    EXPECT_EQ(result.status, BattleActionStatus::Applied);
    EXPECT_FALSE(result.escape_succeeded);
    EXPECT_EQ(turn_core.currentActorId(), std::optional<BattleUnitId>{2});
}

TEST(BattleActionResolverTest, EscapeSuccessEndsBattle) {
    TurnCore turn_core(makeUnits());
    BattleRuntimeState runtime_state = makeRuntimeState(turn_core);
    BattleActionResolver resolver{[] { return 1; }};

    const BattleActionResult result = resolver.resolve(BattleAction{
        .type = BattleActionType::Escape,
        .actor_id = 1
    }, turn_core, runtime_state);

    EXPECT_EQ(result.status, BattleActionStatus::Applied);
    EXPECT_TRUE(result.escape_succeeded);
    EXPECT_EQ(turn_core.outcome(), BattleOutcome::Escaped);
}

} // namespace
} // namespace game::battle
// NOLINTEND
