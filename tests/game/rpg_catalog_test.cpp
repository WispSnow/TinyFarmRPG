#include <gtest/gtest.h>

#include "appearance_test_fixture_utils.h"
#include "game/data/rpg_catalog.h"

#include <filesystem>
#include <string>

namespace game::data {
namespace {

struct FixturePaths {
    std::filesystem::path manifest{};
    std::filesystem::path skills{};
    std::filesystem::path states{};
    std::filesystem::path enemies{};
    std::filesystem::path troops{};
};

FixturePaths createValidRpgFixture() {
    const auto temp_root = game::test::createUniqueTempDir("rpg_catalog_fixture");
    const auto data_root = temp_root / "rpg";
    std::filesystem::create_directories(data_root);

    game::test::writeTextFile(
        data_root / "manifest.json",
        R"json({
  "schema_version": 1,
  "content_versions": {
    "skills": 1,
    "states": 1,
    "enemies": 1,
    "troops": 1
  },
  "features": {
    "quest": false,
    "shop": false
  },
  "files": {
    "skills": "skills.json",
    "states": "states.json",
    "enemies": "enemies.json",
    "troops": "troops.json"
  }
})json");

    game::test::writeTextFile(
        data_root / "skills.json",
        R"json({
  "skills": [
    {
      "id": "skill.attack",
      "display_name": "Attack",
      "description": "Basic attack",
      "scope": "one_enemy",
      "hit_type": "physical",
      "success_rate": 100,
      "repeats": 1,
      "damage": {
        "type": "hp_damage",
        "formula": "a.atk * 4 - b.def * 2",
        "variance": 20,
        "critical": true
      },
      "effects": []
    }
  ]
})json");

    game::test::writeTextFile(
        data_root / "states.json",
        R"json({
  "states": [
    {
      "id": "state.poison",
      "display_name": "Poison",
      "priority": 40,
      "min_turns": 2,
      "max_turns": 4,
      "traits": [
        { "type": "param_rate", "target": "atk", "value": 0.8 }
      ]
    }
  ]
})json");

    game::test::writeTextFile(
        data_root / "enemies.json",
        R"json({
  "enemies": [
    {
      "id": "enemy.slime",
      "display_name": "Slime",
      "params": {
        "mhp": 50,
        "mmp": 0,
        "atk": 10,
        "def": 8,
        "mat": 1,
        "mdf": 2,
        "agi": 6,
        "luk": 5
      },
      "exp": 10,
      "gold": 5,
      "drops": [
        { "item_id": "item.slime_gel", "chance": 0.5 }
      ],
      "actions": [
        { "skill_id": "skill.attack", "rating": 5 }
      ]
    }
  ]
})json");

    game::test::writeTextFile(
        data_root / "troops.json",
        R"json({
  "troops": [
    {
      "id": "troop.slime_pair",
      "display_name": "Slime Pair",
      "members": [
        { "enemy_id": "enemy.slime", "x": 320.0, "y": 280.0 },
        { "enemy_id": "enemy.slime", "x": 420.0, "y": 300.0 }
      ]
    }
  ]
})json");

    return FixturePaths{
        .manifest = data_root / "manifest.json",
        .skills = data_root / "skills.json",
        .states = data_root / "states.json",
        .enemies = data_root / "enemies.json",
        .troops = data_root / "troops.json"};
}

TEST(RpgCatalogTest, LoadsCoreFilesAndPassesReferenceValidation) {
    const FixturePaths paths = createValidRpgFixture();
    RpgCatalog catalog;

    ASSERT_TRUE(catalog.loadManifest(paths.manifest.string()));
    ASSERT_TRUE(catalog.loadSkills(paths.skills.string()));
    ASSERT_TRUE(catalog.loadStates(paths.states.string()));
    ASSERT_TRUE(catalog.loadEnemies(paths.enemies.string()));
    ASSERT_TRUE(catalog.loadTroops(paths.troops.string()));

    std::string error{};
    ASSERT_TRUE(catalog.validateReferences(error)) << error;

    const auto* skill = catalog.findSkill("skill.attack");
    ASSERT_NE(skill, nullptr);
    EXPECT_EQ(skill->display_name_, "Attack");

    const auto* state = catalog.findState("state.poison");
    ASSERT_NE(state, nullptr);
    EXPECT_EQ(state->min_turns_, 2);

    const auto* enemy = catalog.findEnemy("enemy.slime");
    ASSERT_NE(enemy, nullptr);
    EXPECT_EQ(enemy->exp_reward_, 10);

    const auto* troop = catalog.findTroop("troop.slime_pair");
    ASSERT_NE(troop, nullptr);
    ASSERT_EQ(troop->members_.size(), 2U);
}

TEST(RpgCatalogTest, ValidateFailsOnMissingSkillReference) {
    const FixturePaths paths = createValidRpgFixture();
    game::test::writeTextFile(
        paths.enemies,
        R"json({
  "enemies": [
    {
      "id": "enemy.slime",
      "display_name": "Slime",
      "params": [50, 0, 10, 8, 1, 2, 6, 5],
      "exp": 10,
      "gold": 5,
      "actions": [
        { "skill_id": "skill.missing", "rating": 5 }
      ]
    }
  ]
})json");

    RpgCatalog catalog;
    ASSERT_TRUE(catalog.loadSkills(paths.skills.string()));
    ASSERT_TRUE(catalog.loadEnemies(paths.enemies.string()));
    ASSERT_TRUE(catalog.loadTroops(paths.troops.string()));

    std::string error{};
    EXPECT_FALSE(catalog.validateReferences(error));
    EXPECT_NE(error.find("skill.missing"), std::string::npos);
}

TEST(RpgCatalogTest, ValidateFailsOnMissingEnemyReference) {
    const FixturePaths paths = createValidRpgFixture();
    game::test::writeTextFile(
        paths.troops,
        R"json({
  "troops": [
    {
      "id": "troop.invalid",
      "display_name": "Invalid Troop",
      "members": [
        { "enemy_id": "enemy.ghost", "x": 100.0, "y": 100.0 }
      ]
    }
  ]
})json");

    RpgCatalog catalog;
    ASSERT_TRUE(catalog.loadSkills(paths.skills.string()));
    ASSERT_TRUE(catalog.loadEnemies(paths.enemies.string()));
    ASSERT_TRUE(catalog.loadTroops(paths.troops.string()));

    std::string error{};
    EXPECT_FALSE(catalog.validateReferences(error));
    EXPECT_NE(error.find("enemy.ghost"), std::string::npos);
}

TEST(RpgCatalogTest, LoadSkillsFailsOnDuplicateId) {
    const FixturePaths paths = createValidRpgFixture();
    game::test::writeTextFile(
        paths.skills,
        R"json({
  "skills": [
    {
      "id": "skill.dup",
      "scope": "one_enemy",
      "hit_type": "physical",
      "success_rate": 100,
      "repeats": 1,
      "damage": { "type": "hp_damage", "formula": "1", "variance": 0, "critical": false }
    },
    {
      "id": "skill.dup",
      "scope": "one_enemy",
      "hit_type": "physical",
      "success_rate": 100,
      "repeats": 1,
      "damage": { "type": "hp_damage", "formula": "1", "variance": 0, "critical": false }
    }
  ]
})json");

    RpgCatalog catalog;
    EXPECT_FALSE(catalog.loadSkills(paths.skills.string()));
}

TEST(RpgCatalogTest, LoadSkillsFailsOnInvalidScopeEnum) {
    const FixturePaths paths = createValidRpgFixture();
    game::test::writeTextFile(
        paths.skills,
        R"json({
  "skills": [
    {
      "id": "skill.invalid_scope",
      "scope": "not_a_scope",
      "hit_type": "physical",
      "success_rate": 100,
      "repeats": 1,
      "damage": { "type": "hp_damage", "formula": "1", "variance": 0, "critical": false }
    }
  ]
})json");

    RpgCatalog catalog;
    EXPECT_FALSE(catalog.loadSkills(paths.skills.string()));
}

TEST(RpgCatalogTest, LoadSkillsFailsWhenDamageMissing) {
    const FixturePaths paths = createValidRpgFixture();
    game::test::writeTextFile(
        paths.skills,
        R"json({
  "skills": [
    {
      "id": "skill.no_damage",
      "scope": "one_enemy",
      "hit_type": "physical",
      "success_rate": 100,
      "repeats": 1
    }
  ]
})json");

    RpgCatalog catalog;
    EXPECT_FALSE(catalog.loadSkills(paths.skills.string()));
}

TEST(RpgCatalogTest, LoadSkillsFailsOnInvalidJsonContent) {
    const FixturePaths paths = createValidRpgFixture();
    game::test::writeTextFile(paths.skills, "this is not json");

    RpgCatalog catalog;
    EXPECT_FALSE(catalog.loadSkills(paths.skills.string()));
}

TEST(RpgCatalogTest, LoadEnemiesAcceptsParamArraySyntax) {
    const FixturePaths paths = createValidRpgFixture();
    game::test::writeTextFile(
        paths.enemies,
        R"json({
  "enemies": [
    {
      "id": "enemy.array_params",
      "display_name": "Array Enemy",
      "params": [50, 0, 10, 8, 1, 2, 6, 5],
      "exp": 12,
      "gold": 8,
      "actions": [
        { "skill_id": "skill.attack", "rating": 5 }
      ]
    }
  ]
})json");

    RpgCatalog catalog;
    ASSERT_TRUE(catalog.loadSkills(paths.skills.string()));
    ASSERT_TRUE(catalog.loadEnemies(paths.enemies.string()));

    const auto* enemy = catalog.findEnemy("enemy.array_params");
    ASSERT_NE(enemy, nullptr);
    EXPECT_EQ(enemy->params_[static_cast<std::size_t>(ParamIndex::Def)], 8);
}

} // namespace
} // namespace game::data
