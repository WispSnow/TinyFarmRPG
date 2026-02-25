#include <gtest/gtest.h>

#include "game/save/save_data.h"

#include <nlohmann/json.hpp>

namespace game::save {
namespace {

TEST(SaveDataV3RoundTripTest, SerializeThenDeserializePreservesCoreDataAndDefaults) {
    SaveData input{};
    input.schema_version = SAVE_SCHEMA_VERSION;
    input.timestamp = "1700000000";
    input.world_file = "maps/farm.world";

    input.game_time.day = 7;
    input.game_time.hour = 14.5f;
    input.game_time.minute = 33.0f;
    input.game_time.time_scale = 2.0f;
    input.game_time.paused = true;

    input.player.map_name = "farm";
    input.player.position = Vec2f{12.0f, 34.0f};
    input.player.state.action = "walk";
    input.player.state.direction = "left";
    input.player.hp = 88;
    input.player.gold = 345;

    input.player.inventory.active_page = 1;
    input.player.inventory.slots = {
        ItemStackSaveData{1001u, 3},
        ItemStackSaveData{1002u, 1},
    };

    input.player.hotbar.active_slot = 2;
    input.player.hotbar.inventory_slot_indices = {0, 1, -1, -1, -1};

    input.appearance_state.gender = "female";
    input.appearance_state.slots = {
        {"hair", "Lyria/Brown"},
        {"skin", "2"},
    };

    MapSaveData map{};
    map.map_name = "farm";
    map.last_updated_day = 6;
    map.opened_chests = {2, 3};
    map.tilled_tiles = {Vec2i{1, 2}, Vec2i{3, 4}};
    map.wet_tiles = {Vec2i{5, 6}};
    map.crops = {
        CropSaveData{Vec2i{7, 8}, "turnip", "sprout", 5u, 2},
    };
    input.maps.push_back(map);

    const nlohmann::json json = serialize(input);
    EXPECT_EQ(json[json_keys::SCHEMA_VERSION.data()], SAVE_SCHEMA_VERSION);
    EXPECT_TRUE(json.contains(json_keys::QUEST_STATE.data()));
    EXPECT_TRUE(json[json_keys::QUEST_STATE.data()].is_object());
    EXPECT_TRUE(json.contains(json_keys::SKILL_STATE.data()));
    EXPECT_TRUE(json[json_keys::SKILL_STATE.data()].is_object());
    EXPECT_TRUE(json.contains(json_keys::APPEARANCE_STATE.data()));
    EXPECT_TRUE(json[json_keys::APPEARANCE_STATE.data()].is_object());
    EXPECT_TRUE(json.contains(json_keys::COMBAT_STATE.data()));
    EXPECT_TRUE(json[json_keys::COMBAT_STATE.data()].is_object());

    SaveData output{};
    std::string error;
    ASSERT_TRUE(deserialize(json, output, error)) << error;

    EXPECT_EQ(output.schema_version, SAVE_SCHEMA_VERSION);
    ASSERT_TRUE(output.timestamp.has_value());
    EXPECT_EQ(*output.timestamp, *input.timestamp);
    ASSERT_TRUE(output.world_file.has_value());
    EXPECT_EQ(*output.world_file, *input.world_file);

    EXPECT_EQ(output.game_time.day, input.game_time.day);
    EXPECT_FLOAT_EQ(output.game_time.hour, input.game_time.hour);
    EXPECT_FLOAT_EQ(output.game_time.minute, input.game_time.minute);
    EXPECT_FLOAT_EQ(output.game_time.time_scale, input.game_time.time_scale);
    EXPECT_EQ(output.game_time.paused, input.game_time.paused);

    EXPECT_EQ(output.player.map_name, input.player.map_name);
    EXPECT_FLOAT_EQ(output.player.position.x, input.player.position.x);
    EXPECT_FLOAT_EQ(output.player.position.y, input.player.position.y);
    EXPECT_EQ(output.player.state.action, input.player.state.action);
    EXPECT_EQ(output.player.state.direction, input.player.state.direction);
    EXPECT_EQ(output.player.hp, input.player.hp);
    EXPECT_EQ(output.player.gold, input.player.gold);

    EXPECT_EQ(output.player.inventory.active_page, input.player.inventory.active_page);
    ASSERT_EQ(output.player.inventory.slots.size(), input.player.inventory.slots.size());
    EXPECT_EQ(output.player.inventory.slots[0].item_id, input.player.inventory.slots[0].item_id);
    EXPECT_EQ(output.player.inventory.slots[0].count, input.player.inventory.slots[0].count);
    EXPECT_EQ(output.player.inventory.slots[1].item_id, input.player.inventory.slots[1].item_id);
    EXPECT_EQ(output.player.inventory.slots[1].count, input.player.inventory.slots[1].count);

    EXPECT_EQ(output.player.hotbar.active_slot, input.player.hotbar.active_slot);
    EXPECT_EQ(output.player.hotbar.inventory_slot_indices, input.player.hotbar.inventory_slot_indices);

    EXPECT_EQ(output.appearance_state.gender, input.appearance_state.gender);
    EXPECT_EQ(output.appearance_state.slots, input.appearance_state.slots);

    ASSERT_EQ(output.maps.size(), 1u);
    EXPECT_EQ(output.maps[0].map_name, map.map_name);
    EXPECT_EQ(output.maps[0].last_updated_day, map.last_updated_day);
    EXPECT_EQ(output.maps[0].opened_chests.size(), map.opened_chests.size());
    EXPECT_EQ(output.maps[0].tilled_tiles.size(), map.tilled_tiles.size());
    EXPECT_EQ(output.maps[0].wet_tiles.size(), map.wet_tiles.size());
    EXPECT_EQ(output.maps[0].crops.size(), map.crops.size());
}

} // namespace
} // namespace game::save
