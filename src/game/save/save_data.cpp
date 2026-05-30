#include "save_data.h"

#include <nlohmann/json.hpp>

#include <string_view>
#include <type_traits>

namespace game::save {

namespace {

constexpr std::string_view KEY_SCHEMA_VERSION = json_keys::SCHEMA_VERSION;
constexpr std::string_view KEY_TIMESTAMP = json_keys::TIMESTAMP;
constexpr std::string_view KEY_WORLD_FILE = json_keys::WORLD_FILE;
constexpr std::string_view KEY_GAME_TIME = json_keys::GAME_TIME;
constexpr std::string_view KEY_QUEST_STATE = json_keys::QUEST_STATE;
constexpr std::string_view KEY_SKILL_STATE = json_keys::SKILL_STATE;
constexpr std::string_view KEY_APPEARANCE_STATE = json_keys::APPEARANCE_STATE;
constexpr std::string_view KEY_PARTY_STATE = json_keys::PARTY_STATE;
constexpr std::string_view KEY_EQUIPMENT_STATE = json_keys::EQUIPMENT_STATE;
constexpr std::string_view KEY_PARTY_RUNTIME_STATE = json_keys::PARTY_RUNTIME_STATE;
constexpr std::string_view KEY_COMBAT_STATE = json_keys::COMBAT_STATE;
constexpr std::string_view KEY_SCRIPT_STATE = json_keys::SCRIPT_STATE;
constexpr std::string_view KEY_ACTIVE_QUESTS = json_keys::ACTIVE_QUESTS;
constexpr std::string_view KEY_COMPLETED_QUESTS = json_keys::COMPLETED_QUESTS;
constexpr std::string_view KEY_OBJECTIVE_PROGRESS = json_keys::OBJECTIVE_PROGRESS;
constexpr std::string_view KEY_LEARNED_SKILLS = json_keys::LEARNED_SKILLS;
constexpr std::string_view KEY_SKILL_LEVELS = json_keys::SKILL_LEVELS;
constexpr std::string_view KEY_SKILL_COOLDOWNS = json_keys::SKILL_COOLDOWNS;
constexpr std::string_view KEY_PENDING_BATTLE = json_keys::PENDING_BATTLE;
constexpr std::string_view KEY_TROOP_ID = json_keys::TROOP_ID;
constexpr std::string_view KEY_ACTOR_IDS = json_keys::ACTOR_IDS;
constexpr std::string_view KEY_RECRUITED_ACTOR_IDS = json_keys::RECRUITED_ACTOR_IDS;
constexpr std::string_view KEY_ACTIVE_ACTOR_IDS = json_keys::ACTIVE_ACTOR_IDS;
constexpr std::string_view KEY_MAX_ACTIVE_MEMBERS = json_keys::MAX_ACTIVE_MEMBERS;
constexpr std::string_view KEY_LOADOUTS = json_keys::LOADOUTS;
constexpr std::string_view KEY_ACTOR_STATES = json_keys::ACTOR_STATES;
constexpr std::string_view KEY_CURRENT_HP = json_keys::CURRENT_HP;
constexpr std::string_view KEY_CURRENT_MP = json_keys::CURRENT_MP;
constexpr std::string_view KEY_LEVEL = json_keys::LEVEL;
constexpr std::string_view KEY_TOTAL_EXP = json_keys::TOTAL_EXP;
constexpr std::string_view KEY_ITEM_STOCKS = json_keys::ITEM_STOCKS;
constexpr std::string_view KEY_ESCAPE_ATTEMPT_COUNT = json_keys::ESCAPE_ATTEMPT_COUNT;
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

constexpr std::string_view KEY_SLOTS = "slots";
constexpr std::string_view KEY_ITEM_ID = "item_id";
constexpr std::string_view KEY_COUNT = "count";

constexpr std::string_view KEY_ACTIVE_SLOT = "active_slot";
constexpr std::string_view KEY_INVENTORY_SLOT_INDICES = "inventory_slot_indices";

constexpr std::string_view KEY_LAST_UPDATED_DAY = "last_updated_day";
constexpr std::string_view KEY_OPENED_CHESTS = "opened_chests";
constexpr std::string_view KEY_DEFEATED_ENCOUNTERS = json_keys::DEFEATED_ENCOUNTERS;
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
constexpr std::string_view KEY_PROFILE_ID = json_keys::PROFILE_ID;
constexpr std::string_view KEY_GENDER = "gender";

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

bool readPlaceholderObject(const nlohmann::json& json, std::string_view key, std::string& out_error) {
    if (!json.contains(key)) {
        return true;
    }
    if (!json[key].is_object()) {
        out_error = "SaveData: " + std::string(key) + " is not an object";
        return false;
    }
    return true;
}

nlohmann::json stringIntMapToJson(const std::unordered_map<std::string, int>& values) {
    nlohmann::json result = nlohmann::json::object();
    for (const auto& [key, value] : values) {
        result[key] = value;
    }
    return result;
}

bool readStringArrayField(const nlohmann::json& json,
                          std::string_view key,
                          std::vector<std::string>& out_values,
                          std::string& out_error) {
    out_values.clear();
    if (!json.contains(key)) {
        return true;
    }
    if (!json[key].is_array()) {
        out_error = "SaveData: " + std::string(key) + " is not an array";
        return false;
    }

    for (const auto& value : json[key]) {
        if (!value.is_string()) {
            out_error = "SaveData: " + std::string(key) + " contains a non-string entry";
            return false;
        }
        out_values.push_back(value.get<std::string>());
    }
    return true;
}

bool readStringIntMapField(const nlohmann::json& json,
                           std::string_view key,
                           std::unordered_map<std::string, int>& out_values,
                           std::string& out_error) {
    out_values.clear();
    if (!json.contains(key)) {
        return true;
    }
    if (!json[key].is_object()) {
        out_error = "SaveData: " + std::string(key) + " is not an object";
        return false;
    }

    for (const auto& [name, value] : json[key].items()) {
        if (!value.is_number_integer()) {
            out_error = "SaveData: " + std::string(key) + "." + name + " is not an int";
            return false;
        }
        out_values.insert_or_assign(name, value.get<int>());
    }
    return true;
}

bool readEquipmentLoadouts(const nlohmann::json& json,
                           EquipmentStateSaveData& out_state,
                           std::string& out_error) {
    out_state.loadouts.clear();
    if (!json.contains(KEY_LOADOUTS)) {
        return true;
    }
    if (!json[KEY_LOADOUTS].is_object()) {
        out_error = "SaveData: equipment_state.loadouts is not an object";
        return false;
    }

    for (const auto& [actor_id, loadout_json] : json[KEY_LOADOUTS].items()) {
        if (!loadout_json.is_object()) {
            out_error = "SaveData: equipment_state.loadouts." + actor_id + " is not an object";
            return false;
        }
        ActorEquipmentSaveData loadout{};
        for (const auto& [slot, item_id_json] : loadout_json.items()) {
            if (!item_id_json.is_number_unsigned()) {
                out_error = "SaveData: equipment_state.loadouts." + actor_id + "." + slot + " is not an unsigned int";
                return false;
            }
            const auto item_id = item_id_json.get<std::uint64_t>();
            if (item_id != 0U) {
                loadout.slots.emplace(slot, item_id);
            }
        }
        out_state.loadouts.emplace(actor_id, std::move(loadout));
    }
    return true;
}

bool readPartyRuntimeState(const nlohmann::json& json,
                           PartyRuntimeStateSaveData& out_state,
                           std::string& out_error) {
    out_state.actor_states.clear();
    if (!json.contains(KEY_ACTOR_STATES)) {
        return true;
    }
    if (!json[KEY_ACTOR_STATES].is_object()) {
        out_error = "SaveData: party_runtime_state.actor_states is not an object";
        return false;
    }

    for (const auto& [actor_id, state_json] : json[KEY_ACTOR_STATES].items()) {
        if (!state_json.is_object()) {
            out_error = "SaveData: party_runtime_state.actor_states." + actor_id + " is not an object";
            return false;
        }
        ActorRuntimeStateSaveData state{};
        state.current_hp = state_json.value<int>(KEY_CURRENT_HP.data(), 0);
        state.current_mp = state_json.value<int>(KEY_CURRENT_MP.data(), 0);
        state.level = state_json.value<int>(KEY_LEVEL.data(), 1);
        state.total_exp = state_json.value<int>(KEY_TOTAL_EXP.data(), 0);
        out_state.actor_states.emplace(actor_id, state);
    }
    return true;
}

nlohmann::json scriptStateValueToJson(const game::script::ScriptStateValue& value) {
    return std::visit(
        [](const auto& stored_value) -> nlohmann::json {
            using Value = std::decay_t<decltype(stored_value)>;
            if constexpr (std::is_same_v<Value, std::nullptr_t>) {
                return nullptr;
            } else {
                return stored_value;
            }
        },
        value);
}

bool readScriptState(const nlohmann::json& json,
                     ScriptStateSaveData& out_state,
                     std::string& out_error) {
    out_state.values.clear();
    if (!json.is_object()) {
        out_error = "SaveData: script_state is not an object";
        return false;
    }

    for (const auto& [key, value] : json.items()) {
        if (value.is_null()) {
            out_state.values.emplace(key, nullptr);
        } else if (value.is_boolean()) {
            out_state.values.emplace(key, value.get<bool>());
        } else if (value.is_number()) {
            out_state.values.emplace(key, value.get<double>());
        } else if (value.is_string()) {
            out_state.values.emplace(key, value.get<std::string>());
        } else {
            out_error = "SaveData: script_state." + key + " is not a JSON primitive";
            return false;
        }
    }
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
        map_json[KEY_DEFEATED_ENCOUNTERS] = map.defeated_encounters;

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

    root[KEY_QUEST_STATE] = nlohmann::json{
        {KEY_ACTIVE_QUESTS, data.quest_state.active_quests},
        {KEY_COMPLETED_QUESTS, data.quest_state.completed_quests},
        {KEY_OBJECTIVE_PROGRESS, stringIntMapToJson(data.quest_state.objective_progress)},
    };
    root[KEY_SKILL_STATE] = nlohmann::json{
        {KEY_LEARNED_SKILLS, data.skill_state.learned_skills},
        {KEY_SKILL_LEVELS, stringIntMapToJson(data.skill_state.skill_levels)},
        {KEY_SKILL_COOLDOWNS, stringIntMapToJson(data.skill_state.skill_cooldowns)},
    };
    root[KEY_APPEARANCE_STATE] = nlohmann::json{
        {KEY_PROFILE_ID, data.appearance_state.profile_id},
        {KEY_GENDER, data.appearance_state.gender},
        {KEY_SLOTS, data.appearance_state.slots},
    };
    root[KEY_PARTY_STATE] = nlohmann::json{
        {KEY_RECRUITED_ACTOR_IDS, data.party_state.recruited_actor_ids},
        {KEY_ACTIVE_ACTOR_IDS, data.party_state.active_actor_ids},
        {KEY_MAX_ACTIVE_MEMBERS, data.party_state.max_active_members},
    };
    {
        nlohmann::json loadouts = nlohmann::json::object();
        for (const auto& [actor_id, loadout] : data.equipment_state.loadouts) {
            nlohmann::json slots = nlohmann::json::object();
            for (const auto& [slot, item_id] : loadout.slots) {
                slots[slot] = item_id;
            }
            loadouts[actor_id] = std::move(slots);
        }
        root[KEY_EQUIPMENT_STATE] = nlohmann::json{{KEY_LOADOUTS, std::move(loadouts)}};
    }
    {
        nlohmann::json actor_states = nlohmann::json::object();
        for (const auto& [actor_id, state] : data.party_runtime_state.actor_states) {
            actor_states[actor_id] = nlohmann::json{
                {KEY_CURRENT_HP, state.current_hp},
                {KEY_CURRENT_MP, state.current_mp},
                {KEY_LEVEL, state.level},
                {KEY_TOTAL_EXP, state.total_exp},
            };
        }
        root[KEY_PARTY_RUNTIME_STATE] = nlohmann::json{{KEY_ACTOR_STATES, std::move(actor_states)}};
    }
    root[KEY_COMBAT_STATE] = nlohmann::json{
        {KEY_PENDING_BATTLE, data.combat_state.pending_battle},
        {KEY_TROOP_ID, data.combat_state.troop_id},
        {KEY_ACTOR_IDS, data.combat_state.actor_ids},
        {KEY_ITEM_STOCKS, stringIntMapToJson(data.combat_state.item_stocks)},
        {KEY_ESCAPE_ATTEMPT_COUNT, data.combat_state.escape_attempt_count},
    };
    {
        nlohmann::json script_state = nlohmann::json::object();
        for (const auto& [key, value] : data.script_state.values) {
            script_state[key] = scriptStateValueToJson(value);
        }
        root[KEY_SCRIPT_STATE] = std::move(script_state);
    }

    return root;
}

bool deserialize(const nlohmann::json& json, SaveData& out, std::string& out_error) {
    if (!json.is_object()) {
        out_error = "SaveData: root is not an object";
        return false;
    }

    out = SaveData{};
    out.schema_version = json.value<std::uint32_t>(KEY_SCHEMA_VERSION.data(), 0u);
    if (out.schema_version == 0u) {
        out_error = "SaveData: missing schema_version";
        return false;
    }
    if (out.schema_version > SAVE_SCHEMA_VERSION) {
        out_error = "SaveData: unsupported schema_version";
        return false;
    }

    if (json.contains(KEY_TIMESTAMP) && json[KEY_TIMESTAMP].is_string()) {
        out.timestamp = json[KEY_TIMESTAMP].get<std::string>();
    }
    if (json.contains(KEY_WORLD_FILE) && json[KEY_WORLD_FILE].is_string()) {
        out.world_file = json[KEY_WORLD_FILE].get<std::string>();
    }

    if (!json.contains(KEY_GAME_TIME) || !json[KEY_GAME_TIME].is_object()) {
        out_error = "SaveData: missing game_time";
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
        out_error = "SaveData: missing player";
        return false;
    }
    {
        const auto& player = json[KEY_PLAYER];
        out.player.map_name = player.value<std::string>(KEY_MAP_NAME.data(), "");
        if (!player.contains(KEY_POSITION) || !readVec2f(player[KEY_POSITION], out.player.position)) {
            out_error = "SaveData: invalid player.position";
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
            out.player.inventory.slots.clear();
            if (inv.contains(KEY_SLOTS) && inv[KEY_SLOTS].is_array()) {
                for (const auto& slot_json : inv[KEY_SLOTS]) {
                    ItemStackSaveData slot{};
                    if (slot_json.is_object()) {
                        slot.item_id = slot_json.value<std::uint64_t>(KEY_ITEM_ID.data(), 0u);
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

            if (map_json.contains(KEY_DEFEATED_ENCOUNTERS) && map_json[KEY_DEFEATED_ENCOUNTERS].is_array()) {
                for (const auto& id_json : map_json[KEY_DEFEATED_ENCOUNTERS]) {
                    if (id_json.is_number_integer()) {
                        map.defeated_encounters.push_back(id_json.get<int>());
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
                    out_error = "SaveData: maps[].resource_nodes is not an array";
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
                    node.drop_item_id = node_json.value<std::uint64_t>(KEY_DROP_ITEM_ID.data(), 0u);
                    nodes.push_back(std::move(node));
                }
                map.resource_nodes = std::move(nodes);
            }

            out.maps.push_back(std::move(map));
        }
    }

    if (!readPlaceholderObject(json, KEY_QUEST_STATE, out_error)) {
        return false;
    }
    if (json.contains(KEY_QUEST_STATE)) {
        const auto& quest_state = json[KEY_QUEST_STATE];
        if (!readStringArrayField(quest_state, KEY_ACTIVE_QUESTS, out.quest_state.active_quests, out_error)) {
            return false;
        }
        if (!readStringArrayField(quest_state, KEY_COMPLETED_QUESTS, out.quest_state.completed_quests, out_error)) {
            return false;
        }
        if (!readStringIntMapField(quest_state, KEY_OBJECTIVE_PROGRESS, out.quest_state.objective_progress, out_error)) {
            return false;
        }
    }

    if (!readPlaceholderObject(json, KEY_SKILL_STATE, out_error)) {
        return false;
    }
    if (json.contains(KEY_SKILL_STATE)) {
        const auto& skill_state = json[KEY_SKILL_STATE];
        if (!readStringArrayField(skill_state, KEY_LEARNED_SKILLS, out.skill_state.learned_skills, out_error)) {
            return false;
        }
        if (!readStringIntMapField(skill_state, KEY_SKILL_LEVELS, out.skill_state.skill_levels, out_error)) {
            return false;
        }
        if (!readStringIntMapField(skill_state, KEY_SKILL_COOLDOWNS, out.skill_state.skill_cooldowns, out_error)) {
            return false;
        }
    }

    if (!readPlaceholderObject(json, KEY_APPEARANCE_STATE, out_error)) {
        return false;
    }
    if (json.contains(KEY_APPEARANCE_STATE)) {
        const auto& appearance = json[KEY_APPEARANCE_STATE];
        out.appearance_state.profile_id = appearance.value<std::string>(KEY_PROFILE_ID.data(), "player_default");
        out.appearance_state.gender = appearance.value<std::string>(KEY_GENDER.data(), "male");
        out.appearance_state.slots.clear();
        if (appearance.contains(KEY_SLOTS)) {
            if (!appearance[KEY_SLOTS].is_object()) {
                out_error = "SaveData: appearance_state.slots is not an object";
                return false;
            }
            for (const auto& [slot, variant] : appearance[KEY_SLOTS].items()) {
                if (variant.is_string()) {
                    out.appearance_state.slots.emplace(slot, variant.get<std::string>());
                }
            }
        }
    }
    if (!readPlaceholderObject(json, KEY_PARTY_STATE, out_error)) {
        return false;
    }
    if (json.contains(KEY_PARTY_STATE)) {
        const auto& party_state = json[KEY_PARTY_STATE];
        if (!readStringArrayField(party_state, KEY_RECRUITED_ACTOR_IDS, out.party_state.recruited_actor_ids, out_error)) {
            return false;
        }
        if (!readStringArrayField(party_state, KEY_ACTIVE_ACTOR_IDS, out.party_state.active_actor_ids, out_error)) {
            return false;
        }
        if (party_state.contains(KEY_MAX_ACTIVE_MEMBERS)) {
            if (!party_state[KEY_MAX_ACTIVE_MEMBERS].is_number_unsigned()) {
                out_error = "SaveData: party_state.max_active_members is not an unsigned int";
                return false;
            }
            const auto max_active_members = party_state[KEY_MAX_ACTIVE_MEMBERS].get<std::size_t>();
            out.party_state.max_active_members = max_active_members == 0U ? 1U : max_active_members;
        }
    }
    if (!readPlaceholderObject(json, KEY_EQUIPMENT_STATE, out_error)) {
        return false;
    }
    if (json.contains(KEY_EQUIPMENT_STATE) &&
        !readEquipmentLoadouts(json[KEY_EQUIPMENT_STATE], out.equipment_state, out_error)) {
        return false;
    }
    if (!readPlaceholderObject(json, KEY_PARTY_RUNTIME_STATE, out_error)) {
        return false;
    }
    if (json.contains(KEY_PARTY_RUNTIME_STATE) &&
        !readPartyRuntimeState(json[KEY_PARTY_RUNTIME_STATE], out.party_runtime_state, out_error)) {
        return false;
    }
    if (!readPlaceholderObject(json, KEY_COMBAT_STATE, out_error)) {
        return false;
    }
    if (json.contains(KEY_COMBAT_STATE)) {
        const auto& combat_state = json[KEY_COMBAT_STATE];
        out.combat_state.pending_battle = combat_state.value<bool>(KEY_PENDING_BATTLE.data(), false);
        out.combat_state.troop_id = combat_state.value<std::string>(KEY_TROOP_ID.data(), std::string{});
        out.combat_state.escape_attempt_count =
            combat_state.value<std::uint32_t>(KEY_ESCAPE_ATTEMPT_COUNT.data(), 0u);
        if (!readStringArrayField(combat_state, KEY_ACTOR_IDS, out.combat_state.actor_ids, out_error)) {
            return false;
        }
        if (!readStringIntMapField(combat_state, KEY_ITEM_STOCKS, out.combat_state.item_stocks, out_error)) {
            return false;
        }
    }
    if (json.contains(KEY_SCRIPT_STATE) &&
        !readScriptState(json[KEY_SCRIPT_STATE], out.script_state, out_error)) {
        return false;
    }

    return true;
}

} // namespace game::save
