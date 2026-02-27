#include <gtest/gtest.h>

#include "game/save/save_data.h"
#include "game/save/save_migrator.h"

#include <nlohmann/json.hpp>

namespace game::save {
namespace {

TEST(SaveMigratorTest, V2ToV3FillsNewStateFields) {
    auto json = nlohmann::json::parse(R"({
        "schema_version": 2,
        "game_time": {"day": 5},
        "player": {
            "map_name": "farm",
            "position": {"x": 10.5, "y": 20.5}
        },
        "maps": []
    })");

    std::string error;
    ASSERT_TRUE(migrateToLatest(json, error)) << error;

    EXPECT_EQ(json[json_keys::SCHEMA_VERSION.data()], SAVE_SCHEMA_VERSION);
    EXPECT_TRUE(json.contains(json_keys::QUEST_STATE.data()));
    EXPECT_TRUE(json[json_keys::QUEST_STATE.data()].is_object());
    EXPECT_TRUE(json[json_keys::QUEST_STATE.data()].contains(json_keys::ACTIVE_QUESTS.data()));
    EXPECT_TRUE(json[json_keys::QUEST_STATE.data()][json_keys::ACTIVE_QUESTS.data()].is_array());
    EXPECT_TRUE(json[json_keys::QUEST_STATE.data()].contains(json_keys::COMPLETED_QUESTS.data()));
    EXPECT_TRUE(json[json_keys::QUEST_STATE.data()][json_keys::COMPLETED_QUESTS.data()].is_array());
    EXPECT_TRUE(json[json_keys::QUEST_STATE.data()].contains(json_keys::OBJECTIVE_PROGRESS.data()));
    EXPECT_TRUE(json[json_keys::QUEST_STATE.data()][json_keys::OBJECTIVE_PROGRESS.data()].is_object());
    EXPECT_TRUE(json.contains(json_keys::SKILL_STATE.data()));
    EXPECT_TRUE(json[json_keys::SKILL_STATE.data()].is_object());
    EXPECT_TRUE(json[json_keys::SKILL_STATE.data()].contains(json_keys::LEARNED_SKILLS.data()));
    EXPECT_TRUE(json[json_keys::SKILL_STATE.data()][json_keys::LEARNED_SKILLS.data()].is_array());
    EXPECT_TRUE(json[json_keys::SKILL_STATE.data()].contains(json_keys::SKILL_LEVELS.data()));
    EXPECT_TRUE(json[json_keys::SKILL_STATE.data()][json_keys::SKILL_LEVELS.data()].is_object());
    EXPECT_TRUE(json[json_keys::SKILL_STATE.data()].contains(json_keys::SKILL_COOLDOWNS.data()));
    EXPECT_TRUE(json[json_keys::SKILL_STATE.data()][json_keys::SKILL_COOLDOWNS.data()].is_object());
    EXPECT_TRUE(json.contains(json_keys::APPEARANCE_STATE.data()));
    EXPECT_TRUE(json[json_keys::APPEARANCE_STATE.data()].is_object());
    EXPECT_TRUE(json.contains(json_keys::COMBAT_STATE.data()));
    EXPECT_TRUE(json[json_keys::COMBAT_STATE.data()].is_object());
    EXPECT_TRUE(json[json_keys::COMBAT_STATE.data()].contains(json_keys::PENDING_BATTLE.data()));
    EXPECT_TRUE(json[json_keys::COMBAT_STATE.data()][json_keys::PENDING_BATTLE.data()].is_boolean());
    EXPECT_TRUE(json[json_keys::COMBAT_STATE.data()].contains(json_keys::TROOP_ID.data()));
    EXPECT_TRUE(json[json_keys::COMBAT_STATE.data()][json_keys::TROOP_ID.data()].is_string());
    EXPECT_TRUE(json[json_keys::COMBAT_STATE.data()].contains(json_keys::ACTOR_IDS.data()));
    EXPECT_TRUE(json[json_keys::COMBAT_STATE.data()][json_keys::ACTOR_IDS.data()].is_array());
    EXPECT_TRUE(json[json_keys::COMBAT_STATE.data()].contains(json_keys::ITEM_STOCKS.data()));
    EXPECT_TRUE(json[json_keys::COMBAT_STATE.data()][json_keys::ITEM_STOCKS.data()].is_object());
    EXPECT_TRUE(json[json_keys::COMBAT_STATE.data()].contains(json_keys::ESCAPE_ATTEMPT_COUNT.data()));
    EXPECT_TRUE(json[json_keys::COMBAT_STATE.data()][json_keys::ESCAPE_ATTEMPT_COUNT.data()].is_number_unsigned());

    EXPECT_EQ(json[json_keys::GAME_TIME.data()][json_keys::DAY.data()], 5);
    EXPECT_EQ(json["player"]["map_name"], "farm");
}

TEST(SaveMigratorTest, RejectsInvalidQuestStateFieldTypeInV3) {
    auto json = nlohmann::json::parse(R"({
        "schema_version": 3,
        "game_time": {"day": 5},
        "player": {
            "map_name": "farm",
            "position": {"x": 10.5, "y": 20.5}
        },
        "maps": [],
        "quest_state": {
            "active_quests": {}
        },
        "skill_state": {},
        "appearance_state": {},
        "combat_state": {}
    })");

    std::string error;
    EXPECT_FALSE(migrateToLatest(json, error));
    EXPECT_FALSE(error.empty());
}

TEST(SaveMigratorTest, RejectsMissingOrInvalidSchemaVersion) {
    auto expect_reject = [](nlohmann::json json) {
        std::string error;
        EXPECT_FALSE(migrateToLatest(json, error));
        EXPECT_FALSE(error.empty());
    };

    expect_reject(nlohmann::json::parse(R"({"game_time":{"day":1}})"));
    expect_reject(nlohmann::json::parse(R"({"schema_version":0,"game_time":{"day":1}})"));
    expect_reject(nlohmann::json::parse(R"({"schema_version":99,"game_time":{"day":1}})"));
    expect_reject(nlohmann::json::array());
}

} // namespace
} // namespace game::save
