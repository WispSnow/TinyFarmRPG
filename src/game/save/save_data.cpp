#include "save_data.h"

#include <nlohmann/json.hpp>

#include <string_view>

namespace game::save {

namespace {

constexpr std::string_view KEY_SCHEMA_VERSION = json_keys::SCHEMA_VERSION;
constexpr std::string_view KEY_TIMESTAMP = json_keys::TIMESTAMP;
constexpr std::string_view KEY_WORLD_FILE = json_keys::WORLD_FILE;
constexpr std::string_view KEY_GAME_TIME = json_keys::GAME_TIME;
constexpr std::string_view KEY_PLAYER = "player";
constexpr std::string_view KEY_MAPS = "maps";

constexpr std::string_view KEY_DAY = json_keys::DAY;
constexpr std::string_view KEY_HOUR = "hour";
constexpr std::string_view KEY_MINUTE = "minute";
constexpr std::string_view KEY_TIME_SCALE = "time_scale";
constexpr std::string_view KEY_PAUSED = "paused";

constexpr std::string_view KEY_MAP_NAME = "map_name";
constexpr std::string_view KEY_POSITION = "position";
constexpr std::string_view KEY_STATE = "state";
constexpr std::string_view KEY_INVENTORY = "inventory";
constexpr std::string_view KEY_HOTBAR = "hotbar";
constexpr std::string_view KEY_HP = "hp";
constexpr std::string_view KEY_GOLD = "gold";

constexpr std::string_view KEY_ACTION = "action";
constexpr std::string_view KEY_DIRECTION = "direction";

constexpr std::string_view KEY_ACTIVE_PAGE = "active_page";
constexpr std::string_view KEY_SLOTS = "slots";
constexpr std::string_view KEY_ITEM_ID = "item_id";
constexpr std::string_view KEY_COUNT = "count";

constexpr std::string_view KEY_ACTIVE_SLOT = "active_slot";
constexpr std::string_view KEY_INVENTORY_SLOT_INDICES = "inventory_slot_indices";

constexpr std::string_view KEY_LAST_UPDATED_DAY = "last_updated_day";
constexpr std::string_view KEY_OPENED_CHESTS = "opened_chests";
constexpr std::string_view KEY_TILLED_TILES = "tilled_tiles";
constexpr std::string_view KEY_WET_TILES = "wet_tiles";
constexpr std::string_view KEY_CROPS = "crops";
constexpr std::string_view KEY_RESOURCE_NODES = "resource_nodes";

constexpr std::string_view KEY_TILE = "tile";
constexpr std::string_view KEY_CROP_TYPE = "crop_type";
constexpr std::string_view KEY_GROWTH_STAGE = "growth_stage";
constexpr std::string_view KEY_PLANTED_DAY = "planted_day";
constexpr std::string_view KEY_STAGE_COUNTDOWN = "stage_countdown";

constexpr std::string_view KEY_NODE_TYPE = "node_type";
constexpr std::string_view KEY_HIT_COUNT = "hit_count";
constexpr std::string_view KEY_HITS_TO_BREAK = "hits_to_break";
constexpr std::string_view KEY_DROP_ITEM_ID = "drop_item_id";

constexpr std::string_view KEY_X = "x";
constexpr std::string_view KEY_Y = "y";

nlohmann::json vec2ToJson(Vec2f value) {
    return nlohmann::json{{KEY_X, value.x}, {KEY_Y, value.y}};
}

nlohmann::json vec2ToJson(Vec2i value) {
    return nlohmann::json{{KEY_X, value.x}, {KEY_Y, value.y}};
}

bool readVec2f(const nlohmann::json& json, Vec2f& out) {
    if (!json.is_object()) {
        return false;
    }
    out.x = json.value<float>(KEY_X.data(), 0.0f);
    out.y = json.value<float>(KEY_Y.data(), 0.0f);
    return true;
}

bool readVec2i(const nlohmann::json& json, Vec2i& out) {
    if (!json.is_object()) {
        return false;
    }
    out.x = json.value<int>(KEY_X.data(), 0);
    out.y = json.value<int>(KEY_Y.data(), 0);
    return true;
}

} // namespace

nlohmann::json serialize(const SaveData& data) {
    nlohmann::json root = nlohmann::json::object();
    root[KEY_SCHEMA_VERSION] = data.schema_version;
    if (data.timestamp) {
        root[KEY_TIMESTAMP] = *data.timestamp;
    }
    if (data.world_file) {
        root[KEY_WORLD_FILE] = *data.world_file;
    }

    root[KEY_GAME_TIME] = nlohmann::json{
        {KEY_DAY, data.game_time.day},
        {KEY_HOUR, data.game_time.hour},
        {KEY_MINUTE, data.game_time.minute},
        {KEY_TIME_SCALE, data.game_time.time_scale},
        {KEY_PAUSED, data.game_time.paused},
    };

    nlohmann::json player = nlohmann::json::object();
    player[KEY_MAP_NAME] = data.player.map_name;
    player[KEY_POSITION] = vec2ToJson(data.player.position);
    player[KEY_HP] = data.player.hp;
    player[KEY_GOLD] = data.player.gold;
    player[KEY_STATE] = nlohmann::json{
        {KEY_ACTION, data.player.state.action},
        {KEY_DIRECTION, data.player.state.direction},
    };

    nlohmann::json inventory = nlohmann::json::object();
    inventory[KEY_ACTIVE_PAGE] = data.player.inventory.active_page;
    inventory[KEY_SLOTS] = nlohmann::json::array();
    for (const auto& slot : data.player.inventory.slots) {
        inventory[KEY_SLOTS].push_back(nlohmann::json{
            {KEY_ITEM_ID, slot.item_id},
            {KEY_COUNT, slot.count},
        });
    }
    player[KEY_INVENTORY] = std::move(inventory);

    nlohmann::json hotbar = nlohmann::json::object();
    hotbar[KEY_ACTIVE_SLOT] = data.player.hotbar.active_slot;
    hotbar[KEY_INVENTORY_SLOT_INDICES] = data.player.hotbar.inventory_slot_indices;
    player[KEY_HOTBAR] = std::move(hotbar);

    root[KEY_PLAYER] = std::move(player);

    root[KEY_MAPS] = nlohmann::json::array();
    for (const auto& map : data.maps) {
        nlohmann::json map_json = nlohmann::json::object();
        map_json[KEY_MAP_NAME] = map.map_name;
        map_json[KEY_LAST_UPDATED_DAY] = map.last_updated_day;
        map_json[KEY_OPENED_CHESTS] = map.opened_chests;

        map_json[KEY_TILLED_TILES] = nlohmann::json::array();
        for (const auto& tile : map.tilled_tiles) {
            map_json[KEY_TILLED_TILES].push_back(vec2ToJson(tile));
        }

        map_json[KEY_WET_TILES] = nlohmann::json::array();
        for (const auto& tile : map.wet_tiles) {
            map_json[KEY_WET_TILES].push_back(vec2ToJson(tile));
        }

        map_json[KEY_CROPS] = nlohmann::json::array();
        for (const auto& crop : map.crops) {
            map_json[KEY_CROPS].push_back(nlohmann::json{
                {KEY_TILE, vec2ToJson(crop.tile)},
                {KEY_CROP_TYPE, crop.crop_type},
                {KEY_GROWTH_STAGE, crop.growth_stage},
                {KEY_PLANTED_DAY, crop.planted_day},
                {KEY_STAGE_COUNTDOWN, crop.stage_countdown},
            });
        }

        if (map.resource_nodes) {
            map_json[KEY_RESOURCE_NODES] = nlohmann::json::array();
            for (const auto& node : *map.resource_nodes) {
                map_json[KEY_RESOURCE_NODES].push_back(nlohmann::json{
                    {KEY_TILE, vec2ToJson(node.tile)},
                    {KEY_NODE_TYPE, node.node_type},
                    {KEY_HIT_COUNT, node.hit_count},
                    {KEY_HITS_TO_BREAK, node.hits_to_break},
                    {KEY_DROP_ITEM_ID, node.drop_item_id},
                });
            }
        }

        root[KEY_MAPS].push_back(std::move(map_json));
    }

    return root;
}

bool deserialize(const nlohmann::json& json, SaveData& out, std::string& out_error) {
    if (!json.is_object()) {
        out_error = "SaveData: 根节点不是 object";
        return false;
    }

    out = SaveData{};
    out.schema_version = json.value<std::uint32_t>(KEY_SCHEMA_VERSION.data(), 0u);
    if (out.schema_version == 0u) {
        out_error = "SaveData: 缺少 schema_version";
        return false;
    }
    if (out.schema_version > SAVE_SCHEMA_VERSION) {
        out_error = "SaveData: schema_version 不支持";
        return false;
    }

    if (json.contains(KEY_TIMESTAMP) && json[KEY_TIMESTAMP].is_string()) {
        out.timestamp = json[KEY_TIMESTAMP].get<std::string>();
    }
    if (json.contains(KEY_WORLD_FILE) && json[KEY_WORLD_FILE].is_string()) {
        out.world_file = json[KEY_WORLD_FILE].get<std::string>();
    }

    if (!json.contains(KEY_GAME_TIME) || !json[KEY_GAME_TIME].is_object()) {
        out_error = "SaveData: 缺少 game_time";
        return false;
    }
    {
        const auto& time = json[KEY_GAME_TIME];
        out.game_time.day = time.value<std::uint32_t>(KEY_DAY.data(), 1u);
        out.game_time.hour = time.value<float>(KEY_HOUR.data(), 6.0f);
        out.game_time.minute = time.value<float>(KEY_MINUTE.data(), 0.0f);
        out.game_time.time_scale = time.value<float>(KEY_TIME_SCALE.data(), 1.0f);
        out.game_time.paused = time.value<bool>(KEY_PAUSED.data(), false);
    }

    if (!json.contains(KEY_PLAYER) || !json[KEY_PLAYER].is_object()) {
        out_error = "SaveData: 缺少 player";
        return false;
    }
    {
        const auto& player = json[KEY_PLAYER];
        out.player.map_name = player.value<std::string>(KEY_MAP_NAME.data(), "");
        if (!player.contains(KEY_POSITION) || !readVec2f(player[KEY_POSITION], out.player.position)) {
            out_error = "SaveData: player.position 无效";
            return false;
        }
        out.player.hp = player.value<int>(KEY_HP.data(), 100);
        out.player.gold = player.value<int>(KEY_GOLD.data(), 0);

        if (player.contains(KEY_STATE) && player[KEY_STATE].is_object()) {
            const auto& state = player[KEY_STATE];
            out.player.state.action = state.value<std::string>(KEY_ACTION.data(), "idle");
            out.player.state.direction = state.value<std::string>(KEY_DIRECTION.data(), "down");
        }

        if (player.contains(KEY_INVENTORY) && player[KEY_INVENTORY].is_object()) {
            const auto& inv = player[KEY_INVENTORY];
            out.player.inventory.active_page = inv.value<int>(KEY_ACTIVE_PAGE.data(), 0);
            out.player.inventory.slots.clear();
            if (inv.contains(KEY_SLOTS) && inv[KEY_SLOTS].is_array()) {
                for (const auto& slot_json : inv[KEY_SLOTS]) {
                    ItemStackSaveData slot{};
                    if (slot_json.is_object()) {
                        slot.item_id = slot_json.value<std::uint32_t>(KEY_ITEM_ID.data(), 0u);
                        slot.count = slot_json.value<int>(KEY_COUNT.data(), 0);
                    }
                    out.player.inventory.slots.push_back(slot);
                }
            }
        }

        if (player.contains(KEY_HOTBAR) && player[KEY_HOTBAR].is_object()) {
            const auto& hotbar = player[KEY_HOTBAR];
            out.player.hotbar.active_slot = hotbar.value<int>(KEY_ACTIVE_SLOT.data(), 0);
            out.player.hotbar.inventory_slot_indices.clear();
            if (hotbar.contains(KEY_INVENTORY_SLOT_INDICES) && hotbar[KEY_INVENTORY_SLOT_INDICES].is_array()) {
                out.player.hotbar.inventory_slot_indices = hotbar[KEY_INVENTORY_SLOT_INDICES].get<std::vector<int>>();
            }
        }
    }

    if (json.contains(KEY_MAPS) && json[KEY_MAPS].is_array()) {
        out.maps.clear();
        for (const auto& map_json : json[KEY_MAPS]) {
            if (!map_json.is_object()) {
                continue;
            }
            MapSaveData map{};
            map.map_name = map_json.value<std::string>(KEY_MAP_NAME.data(), "");
            map.last_updated_day = map_json.value<std::uint32_t>(KEY_LAST_UPDATED_DAY.data(), 0u);

            if (map_json.contains(KEY_OPENED_CHESTS) && map_json[KEY_OPENED_CHESTS].is_array()) {
                for (const auto& id_json : map_json[KEY_OPENED_CHESTS]) {
                    if (id_json.is_number_integer()) {
                        map.opened_chests.push_back(id_json.get<int>());
                    }
                }
            }

            if (map_json.contains(KEY_TILLED_TILES) && map_json[KEY_TILLED_TILES].is_array()) {
                for (const auto& tile_json : map_json[KEY_TILLED_TILES]) {
                    Vec2i tile{};
                    if (readVec2i(tile_json, tile)) {
                        map.tilled_tiles.push_back(tile);
                    }
                }
            }

            if (map_json.contains(KEY_WET_TILES) && map_json[KEY_WET_TILES].is_array()) {
                for (const auto& tile_json : map_json[KEY_WET_TILES]) {
                    Vec2i tile{};
                    if (readVec2i(tile_json, tile)) {
                        map.wet_tiles.push_back(tile);
                    }
                }
            }

            if (map_json.contains(KEY_CROPS) && map_json[KEY_CROPS].is_array()) {
                for (const auto& crop_json : map_json[KEY_CROPS]) {
                    if (!crop_json.is_object()) {
                        continue;
                    }
                    CropSaveData crop{};
                    if (crop_json.contains(KEY_TILE) && crop_json[KEY_TILE].is_object()) {
                        readVec2i(crop_json[KEY_TILE], crop.tile);
                    }
                    crop.crop_type = crop_json.value<std::string>(KEY_CROP_TYPE.data(), "unknown");
                    crop.growth_stage = crop_json.value<std::string>(KEY_GROWTH_STAGE.data(), "seed");
                    crop.planted_day = crop_json.value<std::uint32_t>(KEY_PLANTED_DAY.data(), 0u);
                    crop.stage_countdown = crop_json.value<int>(KEY_STAGE_COUNTDOWN.data(), 0);
                    map.crops.push_back(std::move(crop));
                }
            }

            if (map_json.contains(KEY_RESOURCE_NODES)) {
                if (!map_json[KEY_RESOURCE_NODES].is_array()) {
                    out_error = "SaveData: maps[].resource_nodes 不是 array";
                    return false;
                }
                std::vector<ResourceNodeSaveData> nodes;
                for (const auto& node_json : map_json[KEY_RESOURCE_NODES]) {
                    if (!node_json.is_object()) {
                        continue;
                    }
                    ResourceNodeSaveData node{};
                    if (node_json.contains(KEY_TILE) && node_json[KEY_TILE].is_object()) {
                        readVec2i(node_json[KEY_TILE], node.tile);
                    }
                    node.node_type = node_json.value<std::string>(KEY_NODE_TYPE.data(), "unknown");
                    node.hit_count = node_json.value<int>(KEY_HIT_COUNT.data(), 0);
                    node.hits_to_break = node_json.value<int>(KEY_HITS_TO_BREAK.data(), 0);
                    node.drop_item_id = node_json.value<std::uint32_t>(KEY_DROP_ITEM_ID.data(), 0u);
                    nodes.push_back(std::move(node));
                }
                map.resource_nodes = std::move(nodes);
            }

            out.maps.push_back(std::move(map));
        }
    }

    return true;
}

} // namespace game::save
