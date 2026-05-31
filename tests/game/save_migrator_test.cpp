#include <gtest/gtest.h>

#include "game/save/save_data.h"
#include "game/save/save_migrator.h"

#include <nlohmann/json.hpp>

namespace game::save {
namespace {

TEST(SaveMigratorTest, V2ToLatestFillsNewStateFields) {
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
    EXPECT_TRUE(json[json_keys::APPEARANCE_STATE.data()].contains(json_keys::PROFILE_ID.data()));
    EXPECT_TRUE(json[json_keys::APPEARANCE_STATE.data()][json_keys::PROFILE_ID.data()].is_string());
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
    EXPECT_TRUE(json.contains(json_keys::PARTY_STATE.data()));
    EXPECT_TRUE(json[json_keys::PARTY_STATE.data()].is_object());
    EXPECT_TRUE(json[json_keys::PARTY_STATE.data()].contains(json_keys::RECRUITED_ACTOR_IDS.data()));
    EXPECT_TRUE(json[json_keys::PARTY_STATE.data()][json_keys::RECRUITED_ACTOR_IDS.data()].is_array());
    EXPECT_TRUE(json[json_keys::PARTY_STATE.data()].contains(json_keys::ACTIVE_ACTOR_IDS.data()));
    EXPECT_TRUE(json[json_keys::PARTY_STATE.data()][json_keys::ACTIVE_ACTOR_IDS.data()].is_array());
    EXPECT_TRUE(json[json_keys::PARTY_STATE.data()].contains(json_keys::MAX_ACTIVE_MEMBERS.data()));
    EXPECT_TRUE(json[json_keys::PARTY_STATE.data()][json_keys::MAX_ACTIVE_MEMBERS.data()].is_number_unsigned());
    EXPECT_EQ(
        json[json_keys::PARTY_STATE.data()][json_keys::MAX_ACTIVE_MEMBERS.data()],
        game::defs::kDefaultMaxActivePartyMembers);
    EXPECT_TRUE(json.contains(json_keys::SCRIPT_STATE.data()));
    EXPECT_TRUE(json[json_keys::SCRIPT_STATE.data()].is_object());

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

TEST(SaveMigratorTest, V3ToLatestFillsEquipmentAndRuntimeStateContainers) {
    auto json = nlohmann::json::parse(R"({
        "schema_version": 3,
        "game_time": {"day": 5},
        "player": {
            "map_name": "farm",
            "position": {"x": 10.5, "y": 20.5}
        },
        "maps": [],
        "quest_state": {},
        "skill_state": {},
        "appearance_state": {},
        "party_state": {},
        "combat_state": {}
    })");

    std::string error;
    ASSERT_TRUE(migrateToLatest(json, error)) << error;

    EXPECT_EQ(json[json_keys::SCHEMA_VERSION.data()], SAVE_SCHEMA_VERSION);
    ASSERT_TRUE(json.contains(json_keys::EQUIPMENT_STATE.data()));
    ASSERT_TRUE(json[json_keys::EQUIPMENT_STATE.data()].is_object());
    EXPECT_TRUE(json[json_keys::EQUIPMENT_STATE.data()].contains(json_keys::LOADOUTS.data()));
    EXPECT_TRUE(json[json_keys::EQUIPMENT_STATE.data()][json_keys::LOADOUTS.data()].is_object());
    EXPECT_TRUE(json[json_keys::EQUIPMENT_STATE.data()][json_keys::LOADOUTS.data()].empty());
    ASSERT_TRUE(json.contains(json_keys::PARTY_RUNTIME_STATE.data()));
    ASSERT_TRUE(json[json_keys::PARTY_RUNTIME_STATE.data()].is_object());
    EXPECT_TRUE(json[json_keys::PARTY_RUNTIME_STATE.data()].contains(json_keys::ACTOR_STATES.data()));
    EXPECT_TRUE(json[json_keys::PARTY_RUNTIME_STATE.data()][json_keys::ACTOR_STATES.data()].is_object());
    EXPECT_TRUE(json[json_keys::PARTY_RUNTIME_STATE.data()][json_keys::ACTOR_STATES.data()].empty());
}

TEST(SaveMigratorTest, V4ToV5AddsAppearanceProfileId) {
    auto json = nlohmann::json::parse(R"({
        "schema_version": 4,
        "game_time": {"day": 5},
        "player": {
            "map_name": "farm",
            "position": {"x": 10.5, "y": 20.5}
        },
        "maps": [],
        "quest_state": {},
        "skill_state": {},
        "appearance_state": {
            "gender": "male",
            "slots": {
                "hair": "hair/brown"
            }
        },
        "party_state": {},
        "equipment_state": {},
        "party_runtime_state": {},
        "combat_state": {}
    })");

    std::string error;
    ASSERT_TRUE(migrateToLatest(json, error)) << error;

    EXPECT_EQ(json[json_keys::SCHEMA_VERSION.data()], SAVE_SCHEMA_VERSION);
    ASSERT_TRUE(json.contains(json_keys::APPEARANCE_STATE.data()));
    const auto& appearance = json[json_keys::APPEARANCE_STATE.data()];
    EXPECT_EQ(appearance[json_keys::PROFILE_ID.data()], "player_default");
    EXPECT_EQ(appearance["gender"], "male");
    EXPECT_EQ(appearance["slots"]["hair"], "hair/brown");
}

TEST(SaveMigratorTest, V5ToLatestAddsRuntimeLevelAndTotalExp) {
    auto json = nlohmann::json::parse(R"({
        "schema_version": 5,
        "game_time": {"day": 5},
        "player": {
            "map_name": "farm",
            "position": {"x": 10.5, "y": 20.5}
        },
        "maps": [],
        "quest_state": {},
        "skill_state": {},
        "appearance_state": {},
        "party_state": {},
        "equipment_state": {},
        "party_runtime_state": {
            "actor_states": {
                "actor.player": {
                    "current_hp": 42,
                    "current_mp": 9
                }
            }
        },
        "combat_state": {}
    })");

    std::string error;
    ASSERT_TRUE(migrateToLatest(json, error)) << error;

    EXPECT_EQ(json[json_keys::SCHEMA_VERSION.data()], SAVE_SCHEMA_VERSION);
    const auto& actor_state =
        json[json_keys::PARTY_RUNTIME_STATE.data()][json_keys::ACTOR_STATES.data()]["actor.player"];
    EXPECT_EQ(actor_state[json_keys::LEVEL.data()], 1);
    EXPECT_EQ(actor_state[json_keys::TOTAL_EXP.data()], 0);
    EXPECT_EQ(actor_state[json_keys::CURRENT_HP.data()], 42);
    EXPECT_EQ(actor_state[json_keys::CURRENT_MP.data()], 9);
}

TEST(SaveMigratorTest, V6ToLatestAddsScriptStateAndPreservesPrimitiveValues) {
    auto json = nlohmann::json::parse(R"({
        "schema_version": 6,
        "game_time": {"day": 5},
        "player": {
            "map_name": "farm",
            "position": {"x": 10.5, "y": 20.5}
        },
        "maps": [],
        "quest_state": {},
        "skill_state": {},
        "appearance_state": {},
        "party_state": {},
        "equipment_state": {},
        "party_runtime_state": {},
        "combat_state": {},
        "script_state": {
            "quest.first_delivery.stage": 2,
            "npc.lyria.mood": "happy",
            "flags.met_lyria": true,
            "nullable.marker": null
        }
    })");

    std::string error;
    ASSERT_TRUE(migrateToLatest(json, error)) << error;

    EXPECT_EQ(json[json_keys::SCHEMA_VERSION.data()], SAVE_SCHEMA_VERSION);
    ASSERT_TRUE(json.contains(json_keys::SCRIPT_STATE.data()));
    const auto& script_state = json[json_keys::SCRIPT_STATE.data()];
    EXPECT_EQ(script_state["quest.first_delivery.stage"], 2);
    EXPECT_EQ(script_state["npc.lyria.mood"], "happy");
    EXPECT_EQ(script_state["flags.met_lyria"], true);
    EXPECT_TRUE(script_state["nullable.marker"].is_null());
}

TEST(SaveMigratorTest, V7ToLatestAddsPartyMaxActiveMembers) {
    auto json = nlohmann::json::parse(R"({
        "schema_version": 7,
        "game_time": {"day": 5},
        "player": {
            "map_name": "farm",
            "position": {"x": 10.5, "y": 20.5}
        },
        "maps": [],
        "quest_state": {},
        "skill_state": {},
        "appearance_state": {},
        "party_state": {
            "recruited_actor_ids": ["actor.player"],
            "active_actor_ids": ["actor.player"]
        },
        "equipment_state": {},
        "party_runtime_state": {},
        "combat_state": {},
        "script_state": {}
    })");

    std::string error;
    ASSERT_TRUE(migrateToLatest(json, error)) << error;

    EXPECT_EQ(json[json_keys::SCHEMA_VERSION.data()], SAVE_SCHEMA_VERSION);
    const auto& party_state = json[json_keys::PARTY_STATE.data()];
    EXPECT_EQ(
        party_state[json_keys::MAX_ACTIVE_MEMBERS.data()],
        game::defs::kDefaultMaxActivePartyMembers);
    EXPECT_EQ(party_state[json_keys::ACTIVE_ACTOR_IDS.data()].at(0), "actor.player");
}

TEST(SaveMigratorTest, V6ToLatestRejectsNonPrimitiveScriptStateValue) {
    auto json = nlohmann::json::parse(R"({
        "schema_version": 6,
        "game_time": {"day": 5},
        "player": {
            "map_name": "farm",
            "position": {"x": 10.5, "y": 20.5}
        },
        "maps": [],
        "quest_state": {},
        "skill_state": {},
        "appearance_state": {},
        "party_state": {},
        "equipment_state": {},
        "party_runtime_state": {},
        "combat_state": {},
        "script_state": {
            "bad.table": []
        }
    })");

    std::string error;
    EXPECT_FALSE(migrateToLatest(json, error));
    EXPECT_NE(error.find("script_state.bad.table"), std::string::npos);
}

TEST(SaveMigratorTest, V5ToLatestRejectsInvalidRuntimeProgressFields) {
    auto json = nlohmann::json::parse(R"({
        "schema_version": 5,
        "game_time": {"day": 5},
        "player": {
            "map_name": "farm",
            "position": {"x": 10.5, "y": 20.5}
        },
        "maps": [],
        "quest_state": {},
        "skill_state": {},
        "appearance_state": {},
        "party_state": {},
        "equipment_state": {},
        "party_runtime_state": {
            "actor_states": {
                "actor.player": {
                    "current_hp": 42,
                    "current_mp": 9,
                    "level": "two",
                    "total_exp": 50
                }
            }
        },
        "combat_state": {}
    })");

    std::string error;
    EXPECT_FALSE(migrateToLatest(json, error));
    EXPECT_NE(error.find("level"), std::string::npos);
}

TEST(SaveMigratorTest, RejectsMissingOrInvalidSchemaVersion) {
    auto expect_reject = [](nlohmann::json json) {
        std::string error;
        EXPECT_FALSE(migrateToLatest(json, error));
        EXPECT_FALSE(error.empty());
    };

    expect_reject(nlohmann::json::parse(R"({"game_time":{"day":1}})"));
    expect_reject(nlohmann::json::parse(R"({"schema_version":0,"game_time":{"day":1}})"));
    expect_reject(nlohmann::json::parse(R"({"schema_version":"8","game_time":{"day":1}})"));
    expect_reject(nlohmann::json::parse(R"({"schema_version":99,"game_time":{"day":1}})"));
    expect_reject(nlohmann::json::array());
}

} // namespace
} // namespace game::save
