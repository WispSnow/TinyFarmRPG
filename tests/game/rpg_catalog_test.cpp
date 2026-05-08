#include <gtest/gtest.h>

#include "appearance_test_fixture_utils.h"
#include "game/data/item_catalog.h"
#include "game/data/rpg_catalog.h"

#include <filesystem>
#include <string>

namespace game::data {
namespace {

struct FixturePaths {
    std::filesystem::path manifest{};
    std::filesystem::path items{};
    std::filesystem::path classes{};
    std::filesystem::path actors{};
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
    "classes": 1,
    "actors": 1,
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
    "classes": "classes.json",
    "actors": "actors.json",
    "skills": "skills.json",
    "states": "states.json",
    "enemies": "enemies.json",
    "troops": "troops.json"
  }
})json");

    game::test::writeTextFile(
        data_root / "items.json",
        R"json({
  "items": [
    {
      "id": "item.slime_gel",
      "display_name": "Slime Gel",
      "category": "material"
    }
  ]
})json");

    game::test::writeTextFile(
        data_root / "classes.json",
        R"json({
  "classes": [
    {
      "id": "class.adventurer",
      "display_name": "Adventurer",
      "base_params": [120, 30, 24, 18, 14, 16, 18, 10]
    }
  ]
})json");

    game::test::writeTextFile(
        data_root / "actors.json",
        R"json({
  "actors": [
    {
      "id": "actor.hero",
      "display_name": "Hero",
      "class_id": "class.adventurer",
      "initial_level": 1,
      "max_level": 99,
      "skill_ids": ["skill.attack"],
      "map_actor_id": "hero",
      "portrait": {
        "path": "assets/farm-rpg/Character and Portrait/Portrait/Premade/1.png",
        "x": 0,
        "y": 0,
        "width": 64,
        "height": 64
      }
    }
  ]
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
      "description": "Weakens physical attacks.",
      "icon_key": "poison",
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
      "battle_visual": {
        "sprite_blueprint_id": "slime",
        "idle_animation": "idle_right",
        "scale": 1.8
      },
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
      "battle_background_id": "Grassland",
      "members": [
        { "enemy_id": "enemy.slime", "x": 320.0, "y": 280.0 },
        { "enemy_id": "enemy.slime", "x": 420.0, "y": 300.0 }
      ]
    }
  ]
})json");

    return FixturePaths{
        .manifest = data_root / "manifest.json",
        .items = data_root / "items.json",
        .classes = data_root / "classes.json",
        .actors = data_root / "actors.json",
        .skills = data_root / "skills.json",
        .states = data_root / "states.json",
        .enemies = data_root / "enemies.json",
        .troops = data_root / "troops.json"};
}

TEST(RpgCatalogTest, LoadsCoreFilesAndPassesReferenceValidation) {
    const FixturePaths paths = createValidRpgFixture();
    RpgCatalog catalog;

    ASSERT_TRUE(catalog.loadManifest(paths.manifest.string()));
    ASSERT_TRUE(catalog.loadClasses(paths.classes.string()));
    ASSERT_TRUE(catalog.loadActors(paths.actors.string()));
    ASSERT_TRUE(catalog.loadSkills(paths.skills.string()));
    ASSERT_TRUE(catalog.loadStates(paths.states.string()));
    ASSERT_TRUE(catalog.loadEnemies(paths.enemies.string()));
    ASSERT_TRUE(catalog.loadTroops(paths.troops.string()));

    std::string error{};
    ASSERT_TRUE(catalog.validateReferences(error)) << error;

    const auto* klass = catalog.findClass("class.adventurer");
    ASSERT_NE(klass, nullptr);
    EXPECT_EQ(klass->display_name_, "Adventurer");

    const auto* actor = catalog.findActor("actor.hero");
    ASSERT_NE(actor, nullptr);
    EXPECT_EQ(actor->class_id_, "class.adventurer");
    EXPECT_EQ(actor->map_actor_id_, "hero");
    ASSERT_EQ(actor->skill_ids_.size(), 1U);
    EXPECT_EQ(actor->skill_ids_[0], "skill.attack");
    ASSERT_TRUE(actor->portrait_.valid());
    EXPECT_EQ(actor->portrait_.path_, "assets/farm-rpg/Character and Portrait/Portrait/Premade/1.png");
    EXPECT_EQ(actor->portrait_.width_, 64);
    EXPECT_EQ(actor->portrait_.height_, 64);

    const auto* skill = catalog.findSkill("skill.attack");
    ASSERT_NE(skill, nullptr);
    EXPECT_EQ(skill->display_name_, "Attack");

    const auto* state = catalog.findState("state.poison");
    ASSERT_NE(state, nullptr);
    EXPECT_EQ(state->min_turns_, 2);
    EXPECT_EQ(state->description_, "Weakens physical attacks.");
    EXPECT_EQ(state->icon_key_, "poison");

    const auto* enemy = catalog.findEnemy("enemy.slime");
    ASSERT_NE(enemy, nullptr);
    EXPECT_EQ(enemy->exp_reward_, 10);
    EXPECT_EQ(enemy->battle_visual_.sprite_blueprint_id_, "slime");
    EXPECT_EQ(enemy->battle_visual_.idle_animation_, "idle_right");
    EXPECT_FLOAT_EQ(enemy->battle_visual_.scale_, 1.8F);

    const auto* troop = catalog.findTroop("troop.slime_pair");
    ASSERT_NE(troop, nullptr);
    EXPECT_EQ(troop->battle_background_id_, "Grassland");
    ASSERT_EQ(troop->members_.size(), 2U);
}

TEST(RpgCatalogTest, RejectsInvalidTroopBattleBackgroundId) {
    const FixturePaths paths = createValidRpgFixture();
    game::test::writeTextFile(
        paths.troops,
        R"json({
  "troops": [
    {
      "id": "troop.slime_pair",
      "display_name": "Slime Pair",
      "battle_background_id": "../Grassland",
      "members": [
        { "enemy_id": "enemy.slime", "x": 320.0, "y": 280.0 }
      ]
    }
  ]
})json");

    RpgCatalog catalog;
    ASSERT_TRUE(catalog.loadClasses(paths.classes.string()));
    ASSERT_TRUE(catalog.loadActors(paths.actors.string()));
    ASSERT_TRUE(catalog.loadSkills(paths.skills.string()));
    ASSERT_TRUE(catalog.loadStates(paths.states.string()));
    ASSERT_TRUE(catalog.loadEnemies(paths.enemies.string()));
    ASSERT_TRUE(catalog.loadTroops(paths.troops.string()));

    std::string error{};
    EXPECT_FALSE(catalog.validateReferences(error));
    EXPECT_NE(error.find("invalid battle_background_id"), std::string::npos);
}

TEST(RpgCatalogTest, ProjectAssetsExposeSlimeTroopForMapEncounter) {
    const std::filesystem::path rpg_root =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "assets/data/rpg").lexically_normal();
    RpgCatalog catalog;

    ASSERT_TRUE(catalog.loadManifest((rpg_root / "manifest.json").string()));
    ASSERT_TRUE(catalog.loadClasses((rpg_root / "classes.json").string()));
    ASSERT_TRUE(catalog.loadActors((rpg_root / "actors.json").string()));
    ASSERT_TRUE(catalog.loadSkills((rpg_root / "skills.json").string()));
    ASSERT_TRUE(catalog.loadStates((rpg_root / "states.json").string()));
    ASSERT_TRUE(catalog.loadEnemies((rpg_root / "enemies.json").string()));
    ASSERT_TRUE(catalog.loadTroops((rpg_root / "troops.json").string()));

    std::string error{};
    ASSERT_TRUE(catalog.validateReferences(error)) << error;

    const auto* enemy = catalog.findEnemy("enemy.slime");
    ASSERT_NE(enemy, nullptr);
    EXPECT_EQ(enemy->display_name_, "Slime");
    EXPECT_EQ(enemy->actions_.front().skill_id_, "skill.attack");
    EXPECT_TRUE(enemy->battle_visual_.valid());
    EXPECT_EQ(enemy->battle_visual_.sprite_blueprint_id_, "slime");
    EXPECT_EQ(enemy->battle_visual_.idle_animation_, "idle_right");

    const auto* goblin = catalog.findEnemy("enemy.goblin");
    ASSERT_NE(goblin, nullptr);
    EXPECT_TRUE(goblin->battle_visual_.valid());
    EXPECT_EQ(goblin->battle_visual_.sprite_blueprint_id_, "goblin");

    const auto* gnome = catalog.findEnemy("enemy.gnome");
    ASSERT_NE(gnome, nullptr);
    EXPECT_TRUE(gnome->battle_visual_.valid());
    EXPECT_EQ(gnome->battle_visual_.sprite_blueprint_id_, "gnome");

    const auto* troop = catalog.findTroop("troop.slime");
    ASSERT_NE(troop, nullptr);
    ASSERT_EQ(troop->members_.size(), 2U);
    EXPECT_EQ(troop->members_[0].enemy_id_, "enemy.slime");
    EXPECT_EQ(troop->members_[1].enemy_id_, "enemy.slime");

    const auto* player = catalog.findActor("actor.player");
    ASSERT_NE(player, nullptr);
    EXPECT_EQ(player->display_name_, "Alex");
    EXPECT_EQ(player->map_actor_id_, "player");
    ASSERT_TRUE(player->portrait_.valid());
    EXPECT_EQ(player->portrait_.path_, "assets/farm-rpg/Character and Portrait/Portrait/Premade/1.png");
    EXPECT_EQ(player->portrait_.width_, 64);
    EXPECT_EQ(player->portrait_.height_, 64);

    const auto* lyria = catalog.findActor("actor.lyria");
    ASSERT_NE(lyria, nullptr);
    EXPECT_EQ(lyria->map_actor_id_, "lyria");
    ASSERT_TRUE(lyria->portrait_.valid());
    EXPECT_EQ(lyria->portrait_.path_, "assets/farm-rpg/Character and Portrait/Portrait/Premade/9.png");

    const auto* tori = catalog.findActor("actor.tori");
    ASSERT_NE(tori, nullptr);
    EXPECT_EQ(tori->map_actor_id_, "tori");
    ASSERT_TRUE(tori->portrait_.valid());
    EXPECT_EQ(tori->portrait_.path_, "assets/farm-rpg/Character and Portrait/Portrait/Premade/2.png");
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

TEST(RpgCatalogTest, ValidateFailsOnMissingActorSkillReference) {
    const FixturePaths paths = createValidRpgFixture();
    game::test::writeTextFile(
        paths.actors,
        R"json({
  "actors": [
    {
      "id": "actor.hero",
      "display_name": "Hero",
      "class_id": "class.adventurer",
      "initial_level": 1,
      "max_level": 99,
      "skill_ids": ["skill.missing"]
    }
  ]
})json");

    RpgCatalog catalog;
    ASSERT_TRUE(catalog.loadClasses(paths.classes.string()));
    ASSERT_TRUE(catalog.loadActors(paths.actors.string()));
    ASSERT_TRUE(catalog.loadSkills(paths.skills.string()));

    std::string error{};
    EXPECT_FALSE(catalog.validateReferences(error));
    EXPECT_NE(error.find("skill.missing"), std::string::npos);
}

TEST(RpgCatalogTest, LoadActorsFailsWhenSkillIdsContainsNonStringEntry) {
    const FixturePaths paths = createValidRpgFixture();
    game::test::writeTextFile(
        paths.actors,
        R"json({
  "actors": [
    {
      "id": "actor.hero",
      "display_name": "Hero",
      "class_id": "class.adventurer",
      "initial_level": 1,
      "max_level": 99,
      "skill_ids": ["skill.attack", 42]
    }
  ]
})json");

    RpgCatalog catalog;
    EXPECT_FALSE(catalog.loadActors(paths.actors.string()));
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

TEST(RpgCatalogTest, LoadSkillsFailsOnNegativeMpCost) {
    const FixturePaths paths = createValidRpgFixture();
    game::test::writeTextFile(
        paths.skills,
        R"json({
  "skills": [
    {
      "id": "skill.invalid_mp_cost",
      "scope": "one_enemy",
      "hit_type": "physical",
      "mp_cost": -1,
      "success_rate": 100,
      "repeats": 1,
      "damage": { "type": "hp_damage", "formula": "1", "variance": 0, "critical": false }
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

TEST(RpgCatalogTest, ValidateFailsOnMissingClassReference) {
    const FixturePaths paths = createValidRpgFixture();
    game::test::writeTextFile(
        paths.actors,
        R"json({
  "actors": [
    {
      "id": "actor.hero",
      "display_name": "Hero",
      "class_id": "class.missing",
      "initial_level": 1,
      "max_level": 99
    }
  ]
})json");

    RpgCatalog catalog;
    ASSERT_TRUE(catalog.loadClasses(paths.classes.string()));
    ASSERT_TRUE(catalog.loadActors(paths.actors.string()));

    std::string error{};
    EXPECT_FALSE(catalog.validateReferences(error));
    EXPECT_NE(error.find("class.missing"), std::string::npos);
}

TEST(RpgCatalogTest, ValidateReferencesChecksEnemyDropItemsWhenItemCatalogProvided) {
    const FixturePaths paths = createValidRpgFixture();

    RpgCatalog catalog;
    ASSERT_TRUE(catalog.loadSkills(paths.skills.string()));
    ASSERT_TRUE(catalog.loadEnemies(paths.enemies.string()));
    ASSERT_TRUE(catalog.loadTroops(paths.troops.string()));

    ItemCatalog item_catalog;
    ASSERT_TRUE(item_catalog.loadItemConfig(paths.items.string()));

    std::string error{};
    EXPECT_TRUE(catalog.validateReferences(error, &item_catalog)) << error;
}

TEST(RpgCatalogTest, ValidateReferencesFailsOnMissingEnemyDropItemWhenItemCatalogProvided) {
    const FixturePaths paths = createValidRpgFixture();
    game::test::writeTextFile(
        paths.items,
        R"json({
  "items": [
    {
      "id": "item.other",
      "display_name": "Other Item",
      "category": "material"
    }
  ]
})json");

    RpgCatalog catalog;
    ASSERT_TRUE(catalog.loadSkills(paths.skills.string()));
    ASSERT_TRUE(catalog.loadEnemies(paths.enemies.string()));
    ASSERT_TRUE(catalog.loadTroops(paths.troops.string()));

    ItemCatalog item_catalog;
    ASSERT_TRUE(item_catalog.loadItemConfig(paths.items.string()));

    std::string error{};
    EXPECT_FALSE(catalog.validateReferences(error, &item_catalog));
    EXPECT_NE(error.find("item.slime_gel"), std::string::npos);
}

TEST(RpgCatalogTest, ListActorsReturnsSortedIds) {
    const FixturePaths paths = createValidRpgFixture();
    game::test::writeTextFile(
        paths.actors,
        R"json({
  "actors": [
    {
      "id": "actor.zeta",
      "display_name": "Zeta",
      "class_id": "class.adventurer",
      "initial_level": 1,
      "max_level": 99
    },
    {
      "id": "actor.alpha",
      "display_name": "Alpha",
      "class_id": "class.adventurer",
      "initial_level": 1,
      "max_level": 99
    }
  ]
})json");

    RpgCatalog catalog;
    ASSERT_TRUE(catalog.loadClasses(paths.classes.string()));
    ASSERT_TRUE(catalog.loadActors(paths.actors.string()));

    const auto actors = catalog.listActors();
    ASSERT_EQ(actors.size(), 2U);
    EXPECT_EQ(actors[0]->id_, "actor.alpha");
    EXPECT_EQ(actors[1]->id_, "actor.zeta");
}

TEST(RpgCatalogTest, ListTroopsReturnsSortedIds) {
    const FixturePaths paths = createValidRpgFixture();
    game::test::writeTextFile(
        paths.troops,
        R"json({
  "troops": [
    {
      "id": "troop.zeta",
      "display_name": "Zeta",
      "members": [
        { "enemy_id": "enemy.slime", "x": 100.0, "y": 100.0 }
      ]
    },
    {
      "id": "troop.alpha",
      "display_name": "Alpha",
      "members": [
        { "enemy_id": "enemy.slime", "x": 120.0, "y": 120.0 }
      ]
    }
  ]
})json");

    RpgCatalog catalog;
    ASSERT_TRUE(catalog.loadEnemies(paths.enemies.string()));
    ASSERT_TRUE(catalog.loadTroops(paths.troops.string()));

    const auto troops = catalog.listTroops();
    ASSERT_EQ(troops.size(), 2U);
    EXPECT_EQ(troops[0]->id_, "troop.alpha");
    EXPECT_EQ(troops[1]->id_, "troop.zeta");
}

} // namespace
} // namespace game::data
