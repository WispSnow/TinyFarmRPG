// NOLINTBEGIN
#include <gtest/gtest.h>

#include "battle_catalog_fixture.h"
#include "game/battle/battle_ai_planner.h"
#include "game/data/rpg_catalog.h"

#include <filesystem>
#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace game::battle {
namespace {

[[nodiscard]] BattleUnit makeUnit(const BattleUnitId id,
                                  std::string_view name,
                                  const BattleSide side,
                                  const int hp,
                                  const int max_hp,
                                  const int mp = 0,
                                  const int max_mp = 0) {
    return BattleUnit{
        .id = id,
        .name = std::string{name},
        .side = side,
        .hp = hp,
        .max_hp = max_hp,
        .mp = mp,
        .max_mp = max_mp,
        .attack = 10,
        .defense = 10,
        .magic_attack = 10,
        .magic_defense = 10,
        .speed = 10,
        .luck = 10};
}

[[nodiscard]] std::filesystem::path writeSkillCatalog(std::string_view prefix, std::string_view json) {
    const auto temp_root = game::test::createUniqueTempDir(prefix);
    const auto skills_path = temp_root / "skills.json";
    game::test::writeTextFile(skills_path, json);
    return skills_path;
}

TEST(BattleAiPlannerTest, ChoosesHighestRatedExecutableSkillAndRandomAliveEnemy) {
    const auto fixture = testdata::createCatalogFixture("battle_ai_highest_rating");
    game::data::RpgCatalog catalog;
    ASSERT_TRUE(catalog.loadSkills(fixture.skills.string()));

    const std::vector<BattleUnit> units{
        makeUnit(1, "Hero", BattleSide::Player, 80, 100),
        makeUnit(2, "Mage", BattleSide::Player, 80, 100),
        makeUnit(101, "Goblin Mage", BattleSide::Enemy, 60, 60, 0, 10)};

    game::data::EnemyData enemy{};
    enemy.actions_ = {
        game::data::EnemyActionData{.skill_id_ = "skill.costly", .rating_ = 100},
        game::data::EnemyActionData{.skill_id_ = "skill.fire", .rating_ = 90},
        game::data::EnemyActionData{.skill_id_ = "skill.none_scope", .rating_ = 80}};

    std::mt19937 rng{1};
    const BattleAction action = BattleAiPlanner::planEnemyAction(units[2], enemy, units, catalog, &rng);

    EXPECT_EQ(action.type, BattleActionType::Skill);
    EXPECT_EQ(action.actor_id, 101);
    EXPECT_EQ(action.skill_id, "skill.fire");
    EXPECT_EQ(action.target_id, std::optional<BattleUnitId>{2});
}

TEST(BattleAiPlannerTest, OneEnemySkillRandomizesTargetAmongAliveOpponents) {
    const auto fixture = testdata::createCatalogFixture("battle_ai_random_enemy_target");
    game::data::RpgCatalog catalog;
    ASSERT_TRUE(catalog.loadSkills(fixture.skills.string()));

    const std::vector<BattleUnit> units{
        makeUnit(1, "Hero A", BattleSide::Player, 80, 100),
        makeUnit(2, "Hero B", BattleSide::Player, 80, 100),
        makeUnit(3, "Hero C", BattleSide::Player, 80, 100),
        makeUnit(101, "Goblin Mage", BattleSide::Enemy, 60, 60, 0, 10)};

    game::data::EnemyData enemy{};
    enemy.actions_ = {
        game::data::EnemyActionData{.skill_id_ = "skill.fire", .rating_ = 100}};

    std::mt19937 rng{1};
    const BattleAction action = BattleAiPlanner::planEnemyAction(units[3], enemy, units, catalog, &rng);

    EXPECT_EQ(action.type, BattleActionType::Skill);
    EXPECT_EQ(action.actor_id, 101);
    EXPECT_EQ(action.skill_id, "skill.fire");
    EXPECT_EQ(action.target_id, std::optional<BattleUnitId>{2});
}

TEST(BattleAiPlannerTest, OneAllyRecoveryTargetsMostInjuredAllyInsteadOfSelf) {
    const auto fixture = testdata::createCatalogFixture("battle_ai_one_ally_heal");
    game::data::RpgCatalog catalog;
    ASSERT_TRUE(catalog.loadSkills(fixture.skills.string()));

    const std::vector<BattleUnit> units{
        makeUnit(1, "Hero", BattleSide::Player, 100, 100),
        makeUnit(101, "Shaman", BattleSide::Enemy, 70, 100),
        makeUnit(102, "Guard", BattleSide::Enemy, 20, 100),
        makeUnit(103, "Scout", BattleSide::Enemy, 35, 100)};

    game::data::EnemyData enemy{};
    enemy.actions_ = {
        game::data::EnemyActionData{.skill_id_ = "skill.ally_heal", .rating_ = 100}};

    const BattleAction action = BattleAiPlanner::planEnemyAction(units[1], enemy, units, catalog);

    EXPECT_EQ(action.type, BattleActionType::Skill);
    EXPECT_EQ(action.skill_id, "skill.ally_heal");
    EXPECT_EQ(action.target_id, std::optional<BattleUnitId>{102});
}

TEST(BattleAiPlannerTest, SelfRecoveryFromDamageTypeFallsBackWhenAlreadyFull) {
    const auto fixture = testdata::createCatalogFixture("battle_ai_self_damage_recover");
    game::data::RpgCatalog catalog;
    ASSERT_TRUE(catalog.loadSkills(fixture.skills.string()));

    const std::vector<BattleUnit> units{
        makeUnit(1, "Hero", BattleSide::Player, 70, 100),
        makeUnit(101, "Priest", BattleSide::Enemy, 100, 100)};

    game::data::EnemyData enemy{};
    enemy.actions_ = {
        game::data::EnemyActionData{.skill_id_ = "skill.self_mend", .rating_ = 100}};

    const BattleAction action = BattleAiPlanner::planEnemyAction(units[1], enemy, units, catalog);

    EXPECT_EQ(action.type, BattleActionType::Attack);
    EXPECT_EQ(action.actor_id, 101);
    EXPECT_EQ(action.target_id, std::optional<BattleUnitId>{1});
}

TEST(BattleAiPlannerTest, FallbackAttackRandomizesTargetAmongAliveOpponents) {
    const std::vector<BattleUnit> units{
        makeUnit(1, "Hero A", BattleSide::Player, 80, 100),
        makeUnit(2, "Hero B", BattleSide::Player, 80, 100),
        makeUnit(3, "Hero C", BattleSide::Player, 80, 100),
        makeUnit(101, "Goblin", BattleSide::Enemy, 40, 40)};

    std::mt19937 rng{1};
    const BattleAction action = BattleAiPlanner::planFallbackAction(units[3], units, &rng);

    EXPECT_EQ(action.type, BattleActionType::Attack);
    EXPECT_EQ(action.actor_id, 101);
    EXPECT_EQ(action.target_id, std::optional<BattleUnitId>{2});
}

TEST(BattleAiPlannerTest, SelfRecoveryFromEffectListFallsBackWhenAlreadyFull) {
    const auto skills_path = writeSkillCatalog(
        "battle_ai_self_effect_recover",
        R"json({
  "skills": [
    {
      "id": "skill.flat_patch",
      "display_name": "Flat Patch",
      "scope": "self",
      "hit_type": "certain",
      "mp_cost": 0,
      "success_rate": 100,
      "repeats": 1,
      "damage": { "type": "none", "formula": "0", "variance": 0, "critical": false },
      "effects": [
        { "type": "recover_hp", "count": 10 }
      ]
    }
  ]
})json");
    game::data::RpgCatalog catalog;
    ASSERT_TRUE(catalog.loadSkills(skills_path.string()));

    const std::vector<BattleUnit> units{
        makeUnit(1, "Hero", BattleSide::Player, 50, 100),
        makeUnit(101, "Medic", BattleSide::Enemy, 100, 100)};

    game::data::EnemyData enemy{};
    enemy.actions_ = {
        game::data::EnemyActionData{.skill_id_ = "skill.flat_patch", .rating_ = 100}};

    const BattleAction action = BattleAiPlanner::planEnemyAction(units[1], enemy, units, catalog);

    EXPECT_EQ(action.type, BattleActionType::Attack);
    EXPECT_EQ(action.actor_id, 101);
    EXPECT_EQ(action.target_id, std::optional<BattleUnitId>{1});
}

TEST(BattleAiPlannerTest, AllAlliesMpRecoveryUsesMpDeficitInsteadOfHp) {
    const auto skills_path = writeSkillCatalog(
        "battle_ai_all_allies_mp_recover",
        R"json({
  "skills": [
    {
      "id": "skill.group_focus",
      "display_name": "Group Focus",
      "scope": "all_allies",
      "hit_type": "certain",
      "mp_cost": 0,
      "success_rate": 100,
      "repeats": 1,
      "damage": { "type": "mp_recover", "formula": "10", "variance": 0, "critical": false },
      "effects": []
    }
  ]
})json");
    game::data::RpgCatalog catalog;
    ASSERT_TRUE(catalog.loadSkills(skills_path.string()));

    const std::vector<BattleUnit> units{
        makeUnit(1, "Hero", BattleSide::Player, 100, 100),
        makeUnit(101, "Invoker", BattleSide::Enemy, 100, 100, 10, 10),
        makeUnit(102, "Acolyte", BattleSide::Enemy, 100, 100, 2, 10)};

    game::data::EnemyData enemy{};
    enemy.actions_ = {
        game::data::EnemyActionData{.skill_id_ = "skill.group_focus", .rating_ = 100}};

    const BattleAction action = BattleAiPlanner::planEnemyAction(units[1], enemy, units, catalog);

    EXPECT_EQ(action.type, BattleActionType::Skill);
    EXPECT_EQ(action.skill_id, "skill.group_focus");
    EXPECT_FALSE(action.target_id.has_value());
}

TEST(BattleAiPlannerTest, FallsBackToEndTurnWhenNoOpponentsAreAlive) {
    const auto fixture = testdata::createCatalogFixture("battle_ai_end_turn_fallback");
    game::data::RpgCatalog catalog;
    ASSERT_TRUE(catalog.loadSkills(fixture.skills.string()));

    const std::vector<BattleUnit> units{
        makeUnit(1, "Hero", BattleSide::Player, 0, 100),
        makeUnit(101, "Goblin", BattleSide::Enemy, 40, 40)};

    game::data::EnemyData enemy{};
    enemy.actions_ = {
        game::data::EnemyActionData{.skill_id_ = "skill.fire", .rating_ = 100}};

    const BattleAction action = BattleAiPlanner::planEnemyAction(units[1], enemy, units, catalog);

    EXPECT_EQ(action.type, BattleActionType::EndTurn);
    EXPECT_EQ(action.actor_id, 101);
    EXPECT_FALSE(action.target_id.has_value());
}

} // namespace
} // namespace game::battle
// NOLINTEND
