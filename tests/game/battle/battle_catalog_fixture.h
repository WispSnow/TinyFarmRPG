#pragma once

#include "appearance_test_fixture_utils.h"

#include <filesystem>
#include <string_view>

namespace game::battle::testdata {

struct CatalogFixturePaths {
    std::filesystem::path skills{};
    std::filesystem::path states{};
    std::filesystem::path items{};
};

inline CatalogFixturePaths createCatalogFixture(std::string_view prefix) {
    const auto temp_root = game::test::createUniqueTempDir(prefix);
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
      "mp_cost": 0,
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
    },
    {
      "id": "skill.cleave",
      "display_name": "Cleave",
      "scope": "all_enemies",
      "hit_type": "physical",
      "mp_cost": 0,
      "success_rate": 100,
      "repeats": 1,
      "damage": { "type": "hp_damage", "formula": "12", "variance": 0, "critical": false },
      "effects": []
    },
    {
      "id": "skill.ally_heal",
      "display_name": "Ally Heal",
      "scope": "one_ally",
      "hit_type": "certain",
      "mp_cost": 0,
      "success_rate": 100,
      "repeats": 1,
      "damage": { "type": "hp_recover", "formula": "18", "variance": 0, "critical": false },
      "effects": []
    },
    {
      "id": "skill.team_heal",
      "display_name": "Team Heal",
      "scope": "all_allies",
      "hit_type": "certain",
      "mp_cost": 0,
      "success_rate": 100,
      "repeats": 1,
      "damage": { "type": "hp_recover", "formula": "10", "variance": 0, "critical": false },
      "effects": []
    },
    {
      "id": "skill.self_mend",
      "display_name": "Self Mend",
      "scope": "self",
      "hit_type": "certain",
      "mp_cost": 0,
      "success_rate": 100,
      "repeats": 1,
      "damage": { "type": "hp_recover", "formula": "15", "variance": 0, "critical": false },
      "effects": []
    },
    {
      "id": "skill.none_scope",
      "display_name": "None Scope",
      "scope": "none",
      "hit_type": "certain",
      "mp_cost": 0,
      "success_rate": 100,
      "repeats": 1,
      "damage": { "type": "none", "formula": "0", "variance": 0, "critical": false },
      "effects": []
    },
    {
      "id": "skill.costly",
      "display_name": "Costly",
      "scope": "one_enemy",
      "hit_type": "certain",
      "mp_cost": 6,
      "success_rate": 100,
      "repeats": 1,
      "damage": { "type": "hp_damage", "formula": "10", "variance": 0, "critical": false },
      "effects": []
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
      "icon_id": "consumables/potion",
      "battle_use": {
        "consume": 1,
        "scope": "one_ally",
        "effects": [
          { "type": "recover_hp", "amount": 50 }
        ]
      }
    },
    {
      "id": "item.ether",
      "display_name": "Ether",
      "category": "consumable",
      "icon_id": "consumables/ether",
      "battle_use": {
        "consume": 1,
        "scope": "one_ally",
        "effects": [
          { "type": "recover_mp", "amount": 10 }
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

} // namespace game::battle::testdata
