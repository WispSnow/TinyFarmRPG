// NOLINTBEGIN
#include <gtest/gtest.h>

#include "battle_catalog_fixture.h"
#include "game/battle/battle_action_resolver.h"
#include "game/battle/turn_core.h"
#include "game/data/item_catalog.h"
#include "game/data/rpg_catalog.h"

#include <optional>
#include <string>
#include <utility>
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

std::vector<BattleUnit> makeUnitsWithTwoEnemies() {
    auto units = makeUnits();
    units.push_back(BattleUnit{
        .id = 102,
        .name = "Bat",
        .side = BattleSide::Enemy,
        .hp = 35,
        .max_hp = 35,
        .mp = 0,
        .max_mp = 0,
        .attack = 11,
        .defense = 9,
        .magic_attack = 8,
        .magic_defense = 8,
        .speed = 7,
        .luck = 9});
    return units;
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

TEST(BattleActionResolverTest, AttackReportsActualDamageWhenOverkillingTarget) {
    auto units = makeUnits();
    units[2].hp = 10;
    TurnCore turn_core(std::move(units));
    ASSERT_EQ(turn_core.currentActorId(), std::optional<BattleUnitId>{1});
    BattleRuntimeState runtime_state = makeRuntimeState(turn_core);

    BattleActionResolver resolver{};
    const BattleActionResult result = resolver.resolve(BattleAction{
        .type = BattleActionType::Attack,
        .actor_id = 1,
        .target_id = 101
    }, turn_core, runtime_state);

    EXPECT_EQ(result.status, BattleActionStatus::Applied);
    EXPECT_EQ(result.damage, 10);
    EXPECT_TRUE(result.target_defeated);

    const auto* slime = turn_core.findUnit(101);
    ASSERT_NE(slime, nullptr);
    EXPECT_EQ(slime->hp, 0);
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
    const auto fixture = testdata::createCatalogFixture("battle_action_resolver_fixture");

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

TEST(BattleActionResolverTest, SkillAllEnemiesScopeTargetsAllAliveEnemies) {
    const auto fixture = testdata::createCatalogFixture("battle_action_resolver_scope_all_enemies_fixture");

    game::data::RpgCatalog rpg_catalog;
    ASSERT_TRUE(rpg_catalog.loadSkills(fixture.skills.string()));

    TurnCore turn_core(makeUnitsWithTwoEnemies());
    BattleRuntimeState runtime_state = makeRuntimeState(turn_core);
    BattleActionResolver resolver{BattleActionResolver::Dependencies{
        .rpg_catalog = &rpg_catalog,
        .item_catalog = nullptr}};

    const BattleActionResult result = resolver.resolve(BattleAction{
        .type = BattleActionType::Skill,
        .actor_id = 1,
        .skill_id = "skill.cleave"
    }, turn_core, runtime_state);

    EXPECT_EQ(result.status, BattleActionStatus::Applied);
    EXPECT_EQ(result.damage, 24);
    const auto* slime = turn_core.findUnit(101);
    const auto* bat = turn_core.findUnit(102);
    ASSERT_NE(slime, nullptr);
    ASSERT_NE(bat, nullptr);
    EXPECT_EQ(slime->hp, 28);
    EXPECT_EQ(bat->hp, 23);
}

TEST(BattleActionResolverTest, SkillSelfScopeUsesActorAsTarget) {
    const auto fixture = testdata::createCatalogFixture("battle_action_resolver_scope_self_fixture");

    game::data::RpgCatalog rpg_catalog;
    ASSERT_TRUE(rpg_catalog.loadSkills(fixture.skills.string()));

    auto units = makeUnits();
    units[0].hp = 80;
    TurnCore turn_core(std::move(units));
    BattleRuntimeState runtime_state = makeRuntimeState(turn_core);
    BattleActionResolver resolver{BattleActionResolver::Dependencies{
        .rpg_catalog = &rpg_catalog,
        .item_catalog = nullptr}};

    const BattleActionResult result = resolver.resolve(BattleAction{
        .type = BattleActionType::Skill,
        .actor_id = 1,
        .skill_id = "skill.self_mend"
    }, turn_core, runtime_state);

    EXPECT_EQ(result.status, BattleActionStatus::Applied);
    EXPECT_EQ(result.hp_recovered, 15);
    const auto* hero = turn_core.findUnit(1);
    ASSERT_NE(hero, nullptr);
    EXPECT_EQ(hero->hp, 95);
}

TEST(BattleActionResolverTest, SkillOneAllyScopeAcceptsExplicitAllyTarget) {
    const auto fixture = testdata::createCatalogFixture("battle_action_resolver_scope_one_ally_fixture");

    game::data::RpgCatalog rpg_catalog;
    ASSERT_TRUE(rpg_catalog.loadSkills(fixture.skills.string()));

    auto units = makeUnits();
    units[1].hp = 50;
    TurnCore turn_core(std::move(units));
    BattleRuntimeState runtime_state = makeRuntimeState(turn_core);
    BattleActionResolver resolver{BattleActionResolver::Dependencies{
        .rpg_catalog = &rpg_catalog,
        .item_catalog = nullptr}};

    const BattleActionResult result = resolver.resolve(BattleAction{
        .type = BattleActionType::Skill,
        .actor_id = 1,
        .target_id = 2,
        .skill_id = "skill.ally_heal"
    }, turn_core, runtime_state);

    EXPECT_EQ(result.status, BattleActionStatus::Applied);
    EXPECT_EQ(result.hp_recovered, 18);
    const auto* partner = turn_core.findUnit(2);
    ASSERT_NE(partner, nullptr);
    EXPECT_EQ(partner->hp, 68);
}

TEST(BattleActionResolverTest, SkillAllAlliesScopeTargetsEntireParty) {
    const auto fixture = testdata::createCatalogFixture("battle_action_resolver_scope_all_allies_fixture");

    game::data::RpgCatalog rpg_catalog;
    ASSERT_TRUE(rpg_catalog.loadSkills(fixture.skills.string()));

    auto units = makeUnits();
    units[0].hp = 100;
    units[1].hp = 70;
    TurnCore turn_core(std::move(units));
    BattleRuntimeState runtime_state = makeRuntimeState(turn_core);
    BattleActionResolver resolver{BattleActionResolver::Dependencies{
        .rpg_catalog = &rpg_catalog,
        .item_catalog = nullptr}};

    const BattleActionResult result = resolver.resolve(BattleAction{
        .type = BattleActionType::Skill,
        .actor_id = 1,
        .skill_id = "skill.team_heal"
    }, turn_core, runtime_state);

    EXPECT_EQ(result.status, BattleActionStatus::Applied);
    EXPECT_EQ(result.hp_recovered, 20);
    const auto* hero = turn_core.findUnit(1);
    const auto* partner = turn_core.findUnit(2);
    ASSERT_NE(hero, nullptr);
    ASSERT_NE(partner, nullptr);
    EXPECT_EQ(hero->hp, 110);
    EXPECT_EQ(partner->hp, 80);
}

TEST(BattleActionResolverTest, SkillScopeNoneIsRejected) {
    const auto fixture = testdata::createCatalogFixture("battle_action_resolver_scope_none_fixture");

    game::data::RpgCatalog rpg_catalog;
    ASSERT_TRUE(rpg_catalog.loadSkills(fixture.skills.string()));

    TurnCore turn_core(makeUnits());
    BattleRuntimeState runtime_state = makeRuntimeState(turn_core);
    BattleActionResolver resolver{BattleActionResolver::Dependencies{
        .rpg_catalog = &rpg_catalog,
        .item_catalog = nullptr}};

    const BattleActionResult result = resolver.resolve(BattleAction{
        .type = BattleActionType::Skill,
        .actor_id = 1,
        .skill_id = "skill.none_scope"
    }, turn_core, runtime_state);

    EXPECT_EQ(result.status, BattleActionStatus::Rejected);
    EXPECT_NE(result.failure_reason.find("scope"), std::string::npos);
}

TEST(BattleActionResolverTest, SkillConsumesMpAndRejectsWhenInsufficient) {
    const auto fixture = testdata::createCatalogFixture("battle_action_resolver_mp_fixture");

    game::data::RpgCatalog rpg_catalog;
    ASSERT_TRUE(rpg_catalog.loadSkills(fixture.skills.string()));

    auto units = makeUnits();
    units[0].mp = 10;
    units[0].max_mp = 10;
    TurnCore turn_core(std::move(units));
    BattleRuntimeState runtime_state = makeRuntimeState(turn_core);
    BattleActionResolver resolver{BattleActionResolver::Dependencies{
        .rpg_catalog = &rpg_catalog,
        .item_catalog = nullptr}};

    const BattleActionResult first_cast = resolver.resolve(BattleAction{
        .type = BattleActionType::Skill,
        .actor_id = 1,
        .target_id = 101,
        .skill_id = "skill.costly"
    }, turn_core, runtime_state);
    EXPECT_EQ(first_cast.status, BattleActionStatus::Applied);
    EXPECT_EQ(first_cast.mp_spent, 6);
    ASSERT_NE(turn_core.findUnit(1), nullptr);
    EXPECT_EQ(turn_core.findUnit(1)->mp, 4);

    EXPECT_EQ(resolver.resolve(BattleAction{
                  .type = BattleActionType::EndTurn,
                  .actor_id = 2
              }, turn_core, runtime_state)
                  .status,
              BattleActionStatus::Applied);
    EXPECT_EQ(resolver.resolve(BattleAction{
                  .type = BattleActionType::EndTurn,
                  .actor_id = 101
              }, turn_core, runtime_state)
                  .status,
              BattleActionStatus::Applied);

    const BattleActionResult second_cast = resolver.resolve(BattleAction{
        .type = BattleActionType::Skill,
        .actor_id = 1,
        .target_id = 101,
        .skill_id = "skill.costly"
    }, turn_core, runtime_state);
    EXPECT_EQ(second_cast.status, BattleActionStatus::Rejected);
    EXPECT_NE(second_cast.failure_reason.find("mp"), std::string::npos);
}

TEST(BattleActionResolverTest, SkillMissConsumesMpAdvancesTurnAndSkipsEffects) {
    const auto fixture = testdata::createCatalogFixture("battle_action_resolver_miss_fixture");

    game::data::RpgCatalog rpg_catalog;
    ASSERT_TRUE(rpg_catalog.loadSkills(fixture.skills.string()));

    auto units = makeUnits();
    units[0].mp = 5;
    units[0].max_mp = 5;
    TurnCore turn_core(std::move(units));
    BattleRuntimeState runtime_state = makeRuntimeState(turn_core);
    BattleActionResolver resolver{BattleActionResolver::Dependencies{
        .rpg_catalog = &rpg_catalog,
        .item_catalog = nullptr}};

    const BattleActionResult result = resolver.resolve(BattleAction{
        .type = BattleActionType::Skill,
        .actor_id = 1,
        .target_id = 101,
        .skill_id = "skill.whiff"
    }, turn_core, runtime_state);

    EXPECT_EQ(result.status, BattleActionStatus::Applied);
    EXPECT_TRUE(result.missed);
    EXPECT_EQ(result.mp_spent, 4);
    EXPECT_EQ(result.damage, 0);
    EXPECT_FALSE(result.target_defeated);
    EXPECT_TRUE(result.states_added.empty());
    EXPECT_EQ(turn_core.currentActorId(), std::optional<BattleUnitId>{2});

    const auto* hero = turn_core.findUnit(1);
    ASSERT_NE(hero, nullptr);
    EXPECT_EQ(hero->mp, 1);

    const auto* slime = turn_core.findUnit(101);
    ASSERT_NE(slime, nullptr);
    EXPECT_EQ(slime->hp, 40);
    EXPECT_TRUE(runtime_state.units[101].state_turns_left.empty());
}

TEST(BattleActionResolverTest, ItemConsumesStockAndRecoversHp) {
    const auto fixture = testdata::createCatalogFixture("battle_action_resolver_item_fixture");

    game::data::ItemCatalog item_catalog;
    ASSERT_TRUE(item_catalog.loadItemConfig(fixture.items.string()));

    auto units = makeUnits();
    units[0].hp = 70;
    TurnCore turn_core(std::move(units));
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
    EXPECT_EQ(result.hp_recovered, 50);
    const auto* hero = turn_core.findUnit(1);
    ASSERT_NE(hero, nullptr);
    EXPECT_EQ(hero->hp, 120);
    EXPECT_EQ(turn_core.currentActorId(), std::optional<BattleUnitId>{2});
    EXPECT_EQ(runtime_state.item_stocks.count(game::data::RpgCatalog::hashId("item.potion")), 0U);
}

TEST(BattleActionResolverTest, ItemOneAllyScopeAcceptsExplicitAllyTarget) {
    const auto fixture = testdata::createCatalogFixture("battle_action_resolver_item_explicit_ally_fixture");

    game::data::ItemCatalog item_catalog;
    ASSERT_TRUE(item_catalog.loadItemConfig(fixture.items.string()));

    auto units = makeUnits();
    units[1].hp = 40;
    TurnCore turn_core(std::move(units));
    BattleRuntimeState runtime_state = makeRuntimeState(turn_core);
    runtime_state.item_stocks[game::data::RpgCatalog::hashId("item.potion")] = 1;

    BattleActionResolver resolver{
        BattleActionResolver::Dependencies{
            .rpg_catalog = nullptr,
            .item_catalog = &item_catalog}};

    const BattleActionResult result = resolver.resolve(BattleAction{
        .type = BattleActionType::Item,
        .actor_id = 1,
        .target_id = 2,
        .item_id = "item.potion"
    }, turn_core, runtime_state);

    EXPECT_EQ(result.status, BattleActionStatus::Applied);
    EXPECT_EQ(result.hp_recovered, 50);
    const auto* partner = turn_core.findUnit(2);
    ASSERT_NE(partner, nullptr);
    EXPECT_EQ(partner->hp, 90);
    EXPECT_EQ(runtime_state.item_stocks.count(game::data::RpgCatalog::hashId("item.potion")), 0U);
}

TEST(BattleActionResolverTest, ItemCanRecoverMp) {
    const auto fixture = testdata::createCatalogFixture("battle_action_resolver_item_mp_fixture");

    game::data::ItemCatalog item_catalog;
    ASSERT_TRUE(item_catalog.loadItemConfig(fixture.items.string()));

    auto units = makeUnits();
    units[0].mp = 2;
    units[0].max_mp = 12;
    TurnCore turn_core(std::move(units));
    BattleRuntimeState runtime_state = makeRuntimeState(turn_core);
    runtime_state.item_stocks[game::data::RpgCatalog::hashId("item.ether")] = 1;

    BattleActionResolver resolver{
        BattleActionResolver::Dependencies{
            .rpg_catalog = nullptr,
            .item_catalog = &item_catalog}};

    const BattleActionResult result = resolver.resolve(BattleAction{
        .type = BattleActionType::Item,
        .actor_id = 1,
        .item_id = "item.ether"
    }, turn_core, runtime_state);

    EXPECT_EQ(result.status, BattleActionStatus::Applied);
    EXPECT_EQ(result.mp_recovered, 10);
    const auto* hero = turn_core.findUnit(1);
    ASSERT_NE(hero, nullptr);
    EXPECT_EQ(hero->mp, 12);
    EXPECT_EQ(runtime_state.item_stocks.count(game::data::RpgCatalog::hashId("item.ether")), 0U);
}

TEST(BattleActionResolverTest, ItemRejectsWithoutStockOrValidTargetAndDoesNotConsume) {
    const auto fixture = testdata::createCatalogFixture("battle_action_resolver_item_reject_fixture");

    game::data::ItemCatalog item_catalog;
    ASSERT_TRUE(item_catalog.loadItemConfig(fixture.items.string()));

    auto units = makeUnits();
    units[0].hp = 70;
    TurnCore turn_core(std::move(units));
    BattleRuntimeState runtime_state = makeRuntimeState(turn_core);
    runtime_state.item_stocks[game::data::RpgCatalog::hashId("item.potion")] = 1;

    BattleActionResolver resolver{
        BattleActionResolver::Dependencies{
            .rpg_catalog = nullptr,
            .item_catalog = &item_catalog}};

    const BattleActionResult invalid_target = resolver.resolve(BattleAction{
        .type = BattleActionType::Item,
        .actor_id = 1,
        .target_id = 101,
        .item_id = "item.potion"
    }, turn_core, runtime_state);

    EXPECT_EQ(invalid_target.status, BattleActionStatus::Rejected);
    EXPECT_NE(invalid_target.failure_reason.find("target"), std::string::npos);
    EXPECT_EQ(runtime_state.item_stocks[game::data::RpgCatalog::hashId("item.potion")], 1);
    EXPECT_EQ(turn_core.currentActorId(), std::optional<BattleUnitId>{1});

    runtime_state.item_stocks.clear();
    const BattleActionResult insufficient_stock = resolver.resolve(BattleAction{
        .type = BattleActionType::Item,
        .actor_id = 1,
        .item_id = "item.potion"
    }, turn_core, runtime_state);

    EXPECT_EQ(insufficient_stock.status, BattleActionStatus::Rejected);
    EXPECT_NE(insufficient_stock.failure_reason.find("stock"), std::string::npos);
    EXPECT_EQ(turn_core.currentActorId(), std::optional<BattleUnitId>{1});
}

TEST(BattleActionResolverTest, ItemRejectsWhenBattleUseIsMissing) {
    const auto fixture = testdata::createCatalogFixture("battle_action_resolver_item_no_battle_use_fixture");

    game::data::ItemCatalog item_catalog;
    ASSERT_TRUE(item_catalog.loadItemConfig(fixture.items.string()));

    TurnCore turn_core(makeUnits());
    BattleRuntimeState runtime_state = makeRuntimeState(turn_core);
    runtime_state.item_stocks[game::data::RpgCatalog::hashId("item.empty_bottle")] = 1;

    BattleActionResolver resolver{
        BattleActionResolver::Dependencies{
            .rpg_catalog = nullptr,
            .item_catalog = &item_catalog}};

    const BattleActionResult result = resolver.resolve(BattleAction{
        .type = BattleActionType::Item,
        .actor_id = 1,
        .item_id = "item.empty_bottle"
    }, turn_core, runtime_state);

    EXPECT_EQ(result.status, BattleActionStatus::Rejected);
    EXPECT_NE(result.failure_reason.find("battle"), std::string::npos);
    EXPECT_EQ(runtime_state.item_stocks[game::data::RpgCatalog::hashId("item.empty_bottle")], 1);
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
