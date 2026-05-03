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
    input.quest_state.active_quests = {"quest.main_story", "quest.village_help"};
    input.quest_state.completed_quests = {"quest.tutorial"};
    input.quest_state.objective_progress = {
        {"quest.main_story.step_1", 2},
        {"quest.village_help.collect_wood", 5},
    };
    input.skill_state.learned_skills = {"skill.attack", "skill.fire"};
    input.skill_state.skill_levels = {
        {"skill.attack", 1},
        {"skill.fire", 3},
    };
    input.skill_state.skill_cooldowns = {
        {"skill.fire", 2},
    };
    input.combat_state.pending_battle = true;
    input.combat_state.troop_id = "troop.goblin_pair";
    input.combat_state.actor_ids = {"actor.reed", "actor.priscilla"};
    input.combat_state.item_stocks = {
        {"strawberry_item", 4},
        {"material_stone", 8},
    };
    input.combat_state.escape_attempt_count = 1;

    MapSaveData map{};
    map.map_name = "farm";
    map.last_updated_day = 6;
    map.opened_chests = {2, 3};
    map.defeated_encounters = {1001, 1002};
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
    EXPECT_TRUE(json[json_keys::QUEST_STATE.data()].contains(json_keys::ACTIVE_QUESTS.data()));
    EXPECT_TRUE(json.contains(json_keys::SKILL_STATE.data()));
    EXPECT_TRUE(json[json_keys::SKILL_STATE.data()].is_object());
    EXPECT_TRUE(json[json_keys::SKILL_STATE.data()].contains(json_keys::LEARNED_SKILLS.data()));
    EXPECT_TRUE(json.contains(json_keys::APPEARANCE_STATE.data()));
    EXPECT_TRUE(json[json_keys::APPEARANCE_STATE.data()].is_object());
    EXPECT_TRUE(json.contains(json_keys::COMBAT_STATE.data()));
    EXPECT_TRUE(json[json_keys::COMBAT_STATE.data()].is_object());
    EXPECT_TRUE(json[json_keys::COMBAT_STATE.data()].contains(json_keys::PENDING_BATTLE.data()));
    ASSERT_TRUE(json.contains("player"));
    ASSERT_TRUE(json["player"].contains("inventory"));
    EXPECT_FALSE(json["player"]["inventory"].contains("active_page"));

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

    ASSERT_EQ(output.player.inventory.slots.size(), input.player.inventory.slots.size());
    EXPECT_EQ(output.player.inventory.slots[0].item_id, input.player.inventory.slots[0].item_id);
    EXPECT_EQ(output.player.inventory.slots[0].count, input.player.inventory.slots[0].count);
    EXPECT_EQ(output.player.inventory.slots[1].item_id, input.player.inventory.slots[1].item_id);
    EXPECT_EQ(output.player.inventory.slots[1].count, input.player.inventory.slots[1].count);

    EXPECT_EQ(output.player.hotbar.active_slot, input.player.hotbar.active_slot);
    EXPECT_EQ(output.player.hotbar.inventory_slot_indices, input.player.hotbar.inventory_slot_indices);

    EXPECT_EQ(output.appearance_state.gender, input.appearance_state.gender);
    EXPECT_EQ(output.appearance_state.slots, input.appearance_state.slots);
    EXPECT_EQ(output.quest_state.active_quests, input.quest_state.active_quests);
    EXPECT_EQ(output.quest_state.completed_quests, input.quest_state.completed_quests);
    EXPECT_EQ(output.quest_state.objective_progress, input.quest_state.objective_progress);
    EXPECT_EQ(output.skill_state.learned_skills, input.skill_state.learned_skills);
    EXPECT_EQ(output.skill_state.skill_levels, input.skill_state.skill_levels);
    EXPECT_EQ(output.skill_state.skill_cooldowns, input.skill_state.skill_cooldowns);
    EXPECT_EQ(output.combat_state.pending_battle, input.combat_state.pending_battle);
    EXPECT_EQ(output.combat_state.troop_id, input.combat_state.troop_id);
    EXPECT_EQ(output.combat_state.actor_ids, input.combat_state.actor_ids);
    EXPECT_EQ(output.combat_state.item_stocks, input.combat_state.item_stocks);
    EXPECT_EQ(output.combat_state.escape_attempt_count, input.combat_state.escape_attempt_count);

    ASSERT_EQ(output.maps.size(), 1u);
    EXPECT_EQ(output.maps[0].map_name, map.map_name);
    EXPECT_EQ(output.maps[0].last_updated_day, map.last_updated_day);
    EXPECT_EQ(output.maps[0].opened_chests.size(), map.opened_chests.size());
    EXPECT_EQ(output.maps[0].defeated_encounters, map.defeated_encounters);
    EXPECT_EQ(output.maps[0].tilled_tiles.size(), map.tilled_tiles.size());
    EXPECT_EQ(output.maps[0].wet_tiles.size(), map.wet_tiles.size());
    EXPECT_EQ(output.maps[0].crops.size(), map.crops.size());
}

} // namespace
} // namespace game::save
