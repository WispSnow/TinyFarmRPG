// NOLINTBEGIN
#include <gtest/gtest.h>

#include "appearance_test_fixture_utils.h"
#include "game/battle/battle_unit_factory.h"
#include "game/data/rpg_catalog.h"

#include <filesystem>
#include <string>

namespace game::battle {
namespace {

struct FixturePaths {
    std::filesystem::path classes{};
    std::filesystem::path actors{};
    std::filesystem::path equipment{};
    std::filesystem::path enemies{};
    std::filesystem::path troops{};
};

[[nodiscard]] FixturePaths createFixture() {
    const auto temp_root = game::test::createUniqueTempDir("battle_unit_factory_fixture");
    const auto data_root = temp_root / "rpg";
    std::filesystem::create_directories(data_root);

    game::test::writeTextFile(
        data_root / "classes.json",
        R"json({
  "classes": [
    {
      "id": "class.mage",
      "display_name": "Mage",
      "base_params": [80, 120, 11, 9, 25, 18, 14, 10]
    },
    {
      "id": "class.swordsman",
      "display_name": "Swordsman",
      "base_params": [140, 30, 28, 20, 12, 16, 18, 8]
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
      "class_id": "class.swordsman",
      "initial_level": 1,
      "max_level": 99,
      "skill_ids": ["skill.attack", "skill.cleave"],
      "portrait": {
        "path": "assets/farm-rpg/Character and Portrait/Portrait/Premade/1.png",
        "x": 0,
        "y": 0,
        "width": 64,
        "height": 64
      }
    },
    {
      "id": "actor.mage",
      "display_name": "Mage",
      "class_id": "class.mage",
      "initial_level": 1,
      "max_level": 99,
      "skill_ids": ["skill.fire"],
      "portrait": {
        "path": "assets/farm-rpg/Character and Portrait/Portrait/Premade/9.png",
        "x": 0,
        "y": 0,
        "width": 64,
        "height": 64
      }
    }
  ]
})json");

    game::test::writeTextFile(
        data_root / "enemies.json",
        R"json({
  "enemies": [
    {
      "id": "enemy.goblin",
      "display_name": "Goblin",
      "params": [60, 0, 12, 8, 5, 5, 10, 5],
      "exp": 5,
      "gold": 2,
      "actions": [
        { "skill_id": "skill.attack", "rating": 5 },
        { "skill_id": "skill.bash", "rating": 3 },
        { "skill_id": "skill.attack", "rating": 1 }
      ]
    },
    {
      "id": "enemy.wolf",
      "display_name": "Wolf",
      "params": [70, 0, 15, 7, 4, 4, 16, 6],
      "exp": 8,
      "gold": 3,
      "actions": [
        { "skill_id": "skill.howl", "rating": 4 }
      ]
    }
  ]
})json");

    game::test::writeTextFile(
        data_root / "troops.json",
        R"json({
  "troops": [
    {
      "id": "troop.goblin_pair",
      "display_name": "Goblin Pair",
      "members": [
        { "enemy_id": "enemy.goblin", "x": 120.0, "y": 160.0 },
        { "enemy_id": "enemy.goblin", "x": 220.0, "y": 160.0 }
      ]
    },
    {
      "id": "troop.wolf_single",
      "display_name": "Wolf Single",
      "members": [
        { "enemy_id": "enemy.wolf", "x": 320.0, "y": 160.0 }
      ]
    }
  ]
})json");

    game::test::writeTextFile(
        data_root / "equipment.json",
        R"json({
  "equipment": [
    {
      "item_id": "equip.hero_sword",
      "slot": "weapon",
      "param_bonuses": {
        "atk": 7,
        "def": 1
      },
      "allowed_actors": ["actor.hero"]
    }
  ]
})json");

    return FixturePaths{
        .classes = data_root / "classes.json",
        .actors = data_root / "actors.json",
        .equipment = data_root / "equipment.json",
        .enemies = data_root / "enemies.json",
        .troops = data_root / "troops.json"};
}

TEST(BattleUnitFactoryTest, BuildsUnitsFromDefaultSelection) {
    const FixturePaths paths = createFixture();
    game::data::RpgCatalog catalog;
    ASSERT_TRUE(catalog.loadClasses(paths.classes.string()));
    ASSERT_TRUE(catalog.loadActors(paths.actors.string()));
    ASSERT_TRUE(catalog.loadEnemies(paths.enemies.string()));
    ASSERT_TRUE(catalog.loadTroops(paths.troops.string()));

    BattleUnitBuildOptions options{};
    std::vector<BattleUnit> units{};
    std::string error{};
    ASSERT_TRUE(buildBattleUnitsFromCatalog(catalog, options, units, error)) << error;

    ASSERT_EQ(units.size(), 4U);
    EXPECT_EQ(units[0].name, "Hero");
    EXPECT_EQ(units[0].max_hp, 140);
    EXPECT_EQ(units[0].max_mp, 30);
    EXPECT_EQ(units[0].attack, 28);
    EXPECT_EQ(units[0].defense, 20);
    EXPECT_EQ(units[0].magic_attack, 12);
    EXPECT_EQ(units[0].magic_defense, 16);
    EXPECT_EQ(units[0].speed, 18);
    EXPECT_EQ(units[0].luck, 8);
    ASSERT_EQ(units[0].skill_ids.size(), 2U);
    EXPECT_EQ(units[0].skill_ids[0], "skill.attack");
    EXPECT_EQ(units[0].skill_ids[1], "skill.cleave");
    ASSERT_TRUE(units[0].source_actor_id.has_value());
    EXPECT_EQ(*units[0].source_actor_id, "actor.hero");
    EXPECT_FALSE(units[0].source_enemy_id.has_value());
    ASSERT_TRUE(units[0].portrait.valid());
    EXPECT_EQ(units[0].portrait.path, "assets/farm-rpg/Character and Portrait/Portrait/Premade/1.png");
    EXPECT_EQ(units[0].portrait.width, 64);
    EXPECT_EQ(units[0].portrait.height, 64);

    EXPECT_EQ(units[1].name, "Mage");
    EXPECT_EQ(units[1].max_hp, 80);
    EXPECT_EQ(units[1].max_mp, 120);
    EXPECT_EQ(units[1].attack, 11);
    EXPECT_EQ(units[1].defense, 9);
    EXPECT_EQ(units[1].magic_attack, 25);
    EXPECT_EQ(units[1].magic_defense, 18);
    EXPECT_EQ(units[1].speed, 14);
    EXPECT_EQ(units[1].luck, 10);
    ASSERT_EQ(units[1].skill_ids.size(), 1U);
    EXPECT_EQ(units[1].skill_ids[0], "skill.fire");
    ASSERT_TRUE(units[1].source_actor_id.has_value());
    EXPECT_EQ(*units[1].source_actor_id, "actor.mage");
    EXPECT_FALSE(units[1].source_enemy_id.has_value());
    ASSERT_TRUE(units[1].portrait.valid());
    EXPECT_EQ(units[1].portrait.path, "assets/farm-rpg/Character and Portrait/Portrait/Premade/9.png");

    EXPECT_EQ(units[2].side, BattleSide::Enemy);
    EXPECT_EQ(units[2].name, "Goblin");
    EXPECT_EQ(units[2].max_mp, 1);
    EXPECT_EQ(units[2].defense, 8);
    EXPECT_EQ(units[2].magic_attack, 5);
    EXPECT_EQ(units[2].magic_defense, 5);
    EXPECT_EQ(units[2].luck, 5);
    ASSERT_EQ(units[2].skill_ids.size(), 2U);
    EXPECT_EQ(units[2].skill_ids[0], "skill.attack");
    EXPECT_EQ(units[2].skill_ids[1], "skill.bash");
    EXPECT_FALSE(units[2].source_actor_id.has_value());
    ASSERT_TRUE(units[2].source_enemy_id.has_value());
    EXPECT_EQ(*units[2].source_enemy_id, "enemy.goblin");
    EXPECT_EQ(units[3].name, "Goblin 2");
    ASSERT_EQ(units[3].skill_ids.size(), 2U);
    EXPECT_EQ(units[3].skill_ids[0], "skill.attack");
    EXPECT_EQ(units[3].skill_ids[1], "skill.bash");
    EXPECT_FALSE(units[3].source_actor_id.has_value());
    ASSERT_TRUE(units[3].source_enemy_id.has_value());
    EXPECT_EQ(*units[3].source_enemy_id, "enemy.goblin");
}

TEST(BattleUnitFactoryTest, SupportsExplicitActorAndTroopSelection) {
    const FixturePaths paths = createFixture();
    game::data::RpgCatalog catalog;
    ASSERT_TRUE(catalog.loadClasses(paths.classes.string()));
    ASSERT_TRUE(catalog.loadActors(paths.actors.string()));
    ASSERT_TRUE(catalog.loadEnemies(paths.enemies.string()));
    ASSERT_TRUE(catalog.loadTroops(paths.troops.string()));

    BattleUnitBuildOptions options{};
    options.actor_ids = {"actor.mage"};
    options.troop_id = "troop.wolf_single";

    std::vector<BattleUnit> units{};
    std::string error{};
    ASSERT_TRUE(buildBattleUnitsFromCatalog(catalog, options, units, error)) << error;

    ASSERT_EQ(units.size(), 2U);
    EXPECT_EQ(units[0].name, "Mage");
    EXPECT_EQ(units[0].side, BattleSide::Player);
    ASSERT_EQ(units[0].skill_ids.size(), 1U);
    EXPECT_EQ(units[0].skill_ids[0], "skill.fire");
    ASSERT_TRUE(units[0].source_actor_id.has_value());
    EXPECT_EQ(*units[0].source_actor_id, "actor.mage");
    EXPECT_FALSE(units[0].source_enemy_id.has_value());
    EXPECT_EQ(units[1].name, "Wolf");
    EXPECT_EQ(units[1].side, BattleSide::Enemy);
    ASSERT_EQ(units[1].skill_ids.size(), 1U);
    EXPECT_EQ(units[1].skill_ids[0], "skill.howl");
    EXPECT_FALSE(units[1].source_actor_id.has_value());
    ASSERT_TRUE(units[1].source_enemy_id.has_value());
    EXPECT_EQ(*units[1].source_enemy_id, "enemy.wolf");
}

TEST(BattleUnitFactoryTest, FailsWhenRequestedActorIsMissing) {
    const FixturePaths paths = createFixture();
    game::data::RpgCatalog catalog;
    ASSERT_TRUE(catalog.loadClasses(paths.classes.string()));
    ASSERT_TRUE(catalog.loadActors(paths.actors.string()));
    ASSERT_TRUE(catalog.loadEnemies(paths.enemies.string()));
    ASSERT_TRUE(catalog.loadTroops(paths.troops.string()));

    BattleUnitBuildOptions options{};
    options.actor_ids = {"actor.missing"};

    std::vector<BattleUnit> units{};
    std::string error{};
    EXPECT_FALSE(buildBattleUnitsFromCatalog(catalog, options, units, error));
    EXPECT_NE(error.find("actor.missing"), std::string::npos);
}

TEST(BattleUnitFactoryTest, AppliesActorEquipmentBonusesWhenBuildingPlayerUnits) {
    const FixturePaths paths = createFixture();
    game::data::RpgCatalog catalog;
    ASSERT_TRUE(catalog.loadClasses(paths.classes.string()));
    ASSERT_TRUE(catalog.loadActors(paths.actors.string()));
    ASSERT_TRUE(catalog.loadEquipment(paths.equipment.string()));
    ASSERT_TRUE(catalog.loadEnemies(paths.enemies.string()));
    ASSERT_TRUE(catalog.loadTroops(paths.troops.string()));

    BattleUnitBuildOptions options{};
    options.actor_ids = {"actor.hero"};
    options.actor_equipment["actor.hero"].equipped_item_ids_[game::data::EquipmentSlotId::Weapon] =
        game::data::RpgCatalog::hashId("equip.hero_sword");

    std::vector<BattleUnit> units{};
    std::string error{};
    ASSERT_TRUE(buildBattleUnitsFromCatalog(catalog, options, units, error)) << error;

    ASSERT_FALSE(units.empty());
    EXPECT_EQ(units[0].attack, 35);
    EXPECT_EQ(units[0].defense, 21);
}

TEST(BattleUnitFactoryTest, FailsWhenRequestedTroopIsMissing) {
    const FixturePaths paths = createFixture();
    game::data::RpgCatalog catalog;
    ASSERT_TRUE(catalog.loadClasses(paths.classes.string()));
    ASSERT_TRUE(catalog.loadActors(paths.actors.string()));
    ASSERT_TRUE(catalog.loadEnemies(paths.enemies.string()));
    ASSERT_TRUE(catalog.loadTroops(paths.troops.string()));

    BattleUnitBuildOptions options{};
    options.troop_id = "troop.missing";

    std::vector<BattleUnit> units{};
    std::string error{};
    EXPECT_FALSE(buildBattleUnitsFromCatalog(catalog, options, units, error));
    EXPECT_NE(error.find("troop.missing"), std::string::npos);
}

} // namespace
} // namespace game::battle
// NOLINTEND
