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
    EXPECT_TRUE(json.contains(json_keys::SKILL_STATE.data()));
    EXPECT_TRUE(json[json_keys::SKILL_STATE.data()].is_object());
    EXPECT_TRUE(json.contains(json_keys::APPEARANCE_STATE.data()));
    EXPECT_TRUE(json[json_keys::APPEARANCE_STATE.data()].is_object());
    EXPECT_TRUE(json.contains(json_keys::COMBAT_STATE.data()));
    EXPECT_TRUE(json[json_keys::COMBAT_STATE.data()].is_object());

    EXPECT_EQ(json[json_keys::GAME_TIME.data()][json_keys::DAY.data()], 5);
    EXPECT_EQ(json["player"]["map_name"], "farm");
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
