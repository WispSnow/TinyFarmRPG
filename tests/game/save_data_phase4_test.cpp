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
    EXPECT_FALSE(data.combat_state.pending_battle);
    EXPECT_TRUE(data.combat_state.troop_id.empty());
    EXPECT_TRUE(data.combat_state.actor_ids.empty());
    EXPECT_TRUE(data.combat_state.item_stocks.empty());
    EXPECT_EQ(data.combat_state.escape_attempt_count, 0u);
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
