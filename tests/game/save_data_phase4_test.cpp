#include <gtest/gtest.h>

#include "game/save/save_data.h"

#include <nlohmann/json.hpp>

namespace game::save {
namespace {

TEST(SaveDataPhase4Test, DeserializeSupportsDefaultExtendedStateFields) {
    const auto json = nlohmann::json::parse(R"json(
{
  "schema_version": 3,
  "game_time": {
    "day": 3,
    "hour": 8.0,
    "minute": 15.0,
    "time_scale": 1.0,
    "paused": false
  },
  "player": {
    "map_name": "farm",
    "position": { "x": 10.0, "y": 20.0 },
    "state": { "action": "idle", "direction": "down" },
    "inventory": { "active_page": 0, "slots": [] },
    "hotbar": { "active_slot": 0, "inventory_slot_indices": [] },
    "hp": 100,
    "gold": 0
  },
  "maps": [],
  "quest_state": {},
  "skill_state": {},
  "appearance_state": {},
  "combat_state": {}
}
)json");

    SaveData data{};
    std::string error{};
    ASSERT_TRUE(deserialize(json, data, error)) << error;

    EXPECT_TRUE(data.quest_state.active_quests.empty());
    EXPECT_TRUE(data.quest_state.completed_quests.empty());
    EXPECT_TRUE(data.quest_state.objective_progress.empty());
    EXPECT_TRUE(data.skill_state.learned_skills.empty());
    EXPECT_TRUE(data.skill_state.skill_levels.empty());
    EXPECT_TRUE(data.skill_state.skill_cooldowns.empty());
    EXPECT_EQ(data.appearance_state.profile_id, "player_default");
    EXPECT_TRUE(data.equipment_state.loadouts.empty());
    EXPECT_TRUE(data.party_runtime_state.actor_states.empty());
    EXPECT_FALSE(data.combat_state.pending_battle);
    EXPECT_TRUE(data.combat_state.troop_id.empty());
    EXPECT_TRUE(data.combat_state.actor_ids.empty());
    EXPECT_TRUE(data.combat_state.item_stocks.empty());
    EXPECT_EQ(data.combat_state.escape_attempt_count, 0u);
}

TEST(SaveDataPhase4Test, RoundtripPreservesEquipmentAndPartyRuntimeState) {
    SaveData source{};
    source.appearance_state.profile_id = "player_default";
    source.equipment_state.loadouts["actor.player"].slots["weapon"] = 1234U;
    source.equipment_state.loadouts["actor.lyria"].slots["body"] = 5678U;
    source.party_runtime_state.actor_states["actor.player"] = ActorRuntimeStateSaveData{
        .current_hp = 321,
        .current_mp = 12,
        .level = 3,
        .total_exp = 150,
    };
    source.party_runtime_state.actor_states["actor.lyria"] = ActorRuntimeStateSaveData{
        .current_hp = 42,
        .current_mp = 77,
        .level = 2,
        .total_exp = 50,
    };

    const nlohmann::json json = serialize(source);

    SaveData loaded{};
    std::string error{};
    ASSERT_TRUE(deserialize(json, loaded, error)) << error;

    EXPECT_EQ(loaded.appearance_state.profile_id, "player_default");
    EXPECT_EQ(loaded.equipment_state.loadouts.at("actor.player").slots.at("weapon"), 1234U);
    EXPECT_EQ(loaded.equipment_state.loadouts.at("actor.lyria").slots.at("body"), 5678U);
    EXPECT_EQ(loaded.party_runtime_state.actor_states.at("actor.player").current_hp, 321);
    EXPECT_EQ(loaded.party_runtime_state.actor_states.at("actor.player").current_mp, 12);
    EXPECT_EQ(loaded.party_runtime_state.actor_states.at("actor.player").level, 3);
    EXPECT_EQ(loaded.party_runtime_state.actor_states.at("actor.player").total_exp, 150);
    EXPECT_EQ(loaded.party_runtime_state.actor_states.at("actor.lyria").current_hp, 42);
    EXPECT_EQ(loaded.party_runtime_state.actor_states.at("actor.lyria").current_mp, 77);
    EXPECT_EQ(loaded.party_runtime_state.actor_states.at("actor.lyria").level, 2);
    EXPECT_EQ(loaded.party_runtime_state.actor_states.at("actor.lyria").total_exp, 50);
}

TEST(SaveDataPhase4Test, DeserializeRejectsInvalidQuestStateFieldType) {
    const auto json = nlohmann::json::parse(R"json(
{
  "schema_version": 3,
  "game_time": { "day": 1 },
  "player": {
    "map_name": "farm",
    "position": { "x": 0.0, "y": 0.0 }
  },
  "maps": [],
  "quest_state": {
    "active_quests": {}
  },
  "skill_state": {},
  "appearance_state": {},
  "combat_state": {}
}
)json");

    SaveData data{};
    std::string error{};
    EXPECT_FALSE(deserialize(json, data, error));
    EXPECT_NE(error.find("active_quests"), std::string::npos);
}

TEST(SaveDataPhase4Test, DeserializeRejectsInvalidCombatStateFieldType) {
    const auto json = nlohmann::json::parse(R"json(
{
  "schema_version": 3,
  "game_time": { "day": 1 },
  "player": {
    "map_name": "farm",
    "position": { "x": 0.0, "y": 0.0 }
  },
  "maps": [],
  "quest_state": {},
  "skill_state": {},
  "appearance_state": {},
  "combat_state": {
    "item_stocks": []
  }
}
)json");

    SaveData data{};
    std::string error{};
    EXPECT_FALSE(deserialize(json, data, error));
    EXPECT_NE(error.find("item_stocks"), std::string::npos);
}

} // namespace
} // namespace game::save
