#include "save_data.h"

#include <nlohmann/json.hpp>

#include <limits>
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

template <typename T>
bool readUnsignedValue(const nlohmann::json& value,
                       T& out,
                       std::string_view path,
                       std::string& out_error) {
    static_assert(std::is_unsigned_v<T>);
    if (!value.is_number_integer()) {
        out_error = "SaveData: " + std::string(path) + " is not an unsigned int";
        return false;
    }

    if (value.is_number_unsigned()) {
        const auto raw = value.get<std::uint64_t>();
        if (raw > static_cast<std::uint64_t>(std::numeric_limits<T>::max())) {
            out_error = "SaveData: " + std::string(path) + " is out of range";
            return false;
        }
        out = static_cast<T>(raw);
        return true;
    }

    const auto raw = value.get<std::int64_t>();
    if (raw < 0 ||
        static_cast<std::uint64_t>(raw) > static_cast<std::uint64_t>(std::numeric_limits<T>::max())) {
        out_error = "SaveData: " + std::string(path) + " is out of range";
        return false;
    }
    out = static_cast<T>(raw);
    return true;
}

template <typename T>
bool readSignedValue(const nlohmann::json& value,
                     T& out,
                     std::string_view path,
                     std::string& out_error) {
    static_assert(std::is_signed_v<T>);
    if (!value.is_number_integer()) {
        out_error = "SaveData: " + std::string(path) + " is not an int";
        return false;
    }

    if (value.is_number_unsigned()) {
        const auto raw = value.get<std::uint64_t>();
        if (raw > static_cast<std::uint64_t>(std::numeric_limits<T>::max())) {
            out_error = "SaveData: " + std::string(path) + " is out of range";
            return false;
        }
        out = static_cast<T>(raw);
        return true;
    }

    const auto raw = value.get<std::int64_t>();
    if (raw < static_cast<std::int64_t>(std::numeric_limits<T>::min()) ||
        raw > static_cast<std::int64_t>(std::numeric_limits<T>::max())) {
        out_error = "SaveData: " + std::string(path) + " is out of range";
        return false;
    }
    out = static_cast<T>(raw);
    return true;
}

bool readFloatValue(const nlohmann::json& value,
                    float& out,
                    std::string_view path,
                    std::string& out_error) {
    if (!value.is_number()) {
        out_error = "SaveData: " + std::string(path) + " is not a number";
        return false;
    }
    out = value.get<float>();
    return true;
}

bool readBoolValue(const nlohmann::json& value,
                   bool& out,
                   std::string_view path,
                   std::string& out_error) {
    if (!value.is_boolean()) {
        out_error = "SaveData: " + std::string(path) + " is not a boolean";
        return false;
    }
    out = value.get<bool>();
    return true;
}

bool readStringValue(const nlohmann::json& value,
                     std::string& out,
                     std::string_view path,
                     std::string& out_error) {
    if (!value.is_string()) {
        out_error = "SaveData: " + std::string(path) + " is not a string";
        return false;
    }
    out = value.get<std::string>();
    return true;
}

template <typename T>
bool readOptionalUnsignedField(const nlohmann::json& json,
                               std::string_view key,
                               T& out,
                               T default_value,
                               std::string_view path,
                               std::string& out_error) {
    if (!json.contains(key)) {
        out = default_value;
        return true;
    }
    return readUnsignedValue(json[key], out, path, out_error);
}

template <typename T>
bool readOptionalSignedField(const nlohmann::json& json,
                             std::string_view key,
                             T& out,
                             T default_value,
                             std::string_view path,
                             std::string& out_error) {
    if (!json.contains(key)) {
        out = default_value;
        return true;
    }
    return readSignedValue(json[key], out, path, out_error);
}

bool readOptionalFloatField(const nlohmann::json& json,
                            std::string_view key,
                            float& out,
                            float default_value,
                            std::string_view path,
                            std::string& out_error) {
    if (!json.contains(key)) {
        out = default_value;
        return true;
    }
    return readFloatValue(json[key], out, path, out_error);
}

bool readOptionalBoolField(const nlohmann::json& json,
                           std::string_view key,
                           bool& out,
                           bool default_value,
                           std::string_view path,
                           std::string& out_error) {
    if (!json.contains(key)) {
        out = default_value;
        return true;
    }
    return readBoolValue(json[key], out, path, out_error);
}

bool readOptionalStringField(const nlohmann::json& json,
                             std::string_view key,
                             std::string& out,
                             std::string_view default_value,
                             std::string_view path,
                             std::string& out_error) {
    if (!json.contains(key)) {
        out = std::string{default_value};
        return true;
    }
    return readStringValue(json[key], out, path, out_error);
}

nlohmann::json vec2ToJson(Vec2f value) {
    return nlohmann::json{{KEY_X, value.x}, {KEY_Y, value.y}};
}

nlohmann::json vec2ToJson(Vec2i value) {
    return nlohmann::json{{KEY_X, value.x}, {KEY_Y, value.y}};
}

bool readVec2f(const nlohmann::json& json,
               Vec2f& out,
               std::string_view path,
               std::string& out_error) {
    if (!json.is_object()) {
        out_error = "SaveData: " + std::string(path) + " is not an object";
        return false;
    }
    return readOptionalFloatField(json, KEY_X, out.x, 0.0f, std::string{path} + ".x", out_error) &&
           readOptionalFloatField(json, KEY_Y, out.y, 0.0f, std::string{path} + ".y", out_error);
}

bool readVec2i(const nlohmann::json& json,
               Vec2i& out,
               std::string_view path,
               std::string& out_error) {
    if (!json.is_object()) {
        out_error = "SaveData: " + std::string(path) + " is not an object";
        return false;
    }
    return readOptionalSignedField(json, KEY_X, out.x, 0, std::string{path} + ".x", out_error) &&
           readOptionalSignedField(json, KEY_Y, out.y, 0, std::string{path} + ".y", out_error);
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
        std::string entry;
        if (!readStringValue(value, entry, std::string{key} + "[]", out_error)) {
            return false;
        }
        out_values.push_back(std::move(entry));
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
        int entry = 0;
        if (!readSignedValue(value, entry, std::string{key} + "." + name, out_error)) {
            return false;
        }
        out_values.insert_or_assign(name, entry);
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
            std::uint64_t item_id = 0;
            if (!readUnsignedValue(item_id_json, item_id, "equipment_state.loadouts." + actor_id + "." + slot, out_error)) {
                return false;
            }
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
        if (!readOptionalSignedField(state_json, KEY_CURRENT_HP, state.current_hp, 0, "party_runtime_state.actor_states." + actor_id + ".current_hp", out_error) ||
            !readOptionalSignedField(state_json, KEY_CURRENT_MP, state.current_mp, 0, "party_runtime_state.actor_states." + actor_id + ".current_mp", out_error) ||
            !readOptionalSignedField(state_json, KEY_LEVEL, state.level, 1, "party_runtime_state.actor_states." + actor_id + ".level", out_error) ||
            !readOptionalSignedField(state_json, KEY_TOTAL_EXP, state.total_exp, 0, "party_runtime_state.actor_states." + actor_id + ".total_exp", out_error)) {
            return false;
        }
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
            bool stored_value = false;
            if (!readBoolValue(value, stored_value, "script_state." + key, out_error)) {
                return false;
            }
            out_state.values.emplace(key, stored_value);
        } else if (value.is_number()) {
            out_state.values.emplace(key, value.get<double>());
        } else if (value.is_string()) {
            std::string stored_value;
            if (!readStringValue(value, stored_value, "script_state." + key, out_error)) {
                return false;
            }
            out_state.values.emplace(key, std::move(stored_value));
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
    if (!json.contains(KEY_SCHEMA_VERSION)) {
        out_error = "SaveData: missing schema_version";
        return false;
    }
    if (!readUnsignedValue(json[KEY_SCHEMA_VERSION], out.schema_version, KEY_SCHEMA_VERSION, out_error)) {
        return false;
    }
    if (out.schema_version == 0u) {
        out_error = "SaveData: missing schema_version";
        return false;
    }
    if (out.schema_version > SAVE_SCHEMA_VERSION) {
        out_error = "SaveData: unsupported schema_version";
        return false;
    }

    if (json.contains(KEY_TIMESTAMP)) {
        std::string timestamp;
        if (!readStringValue(json[KEY_TIMESTAMP], timestamp, KEY_TIMESTAMP, out_error)) {
            return false;
        }
        out.timestamp = std::move(timestamp);
    }
    if (json.contains(KEY_WORLD_FILE)) {
        std::string world_file;
        if (!readStringValue(json[KEY_WORLD_FILE], world_file, KEY_WORLD_FILE, out_error)) {
            return false;
        }
        out.world_file = std::move(world_file);
    }

    if (!json.contains(KEY_GAME_TIME) || !json[KEY_GAME_TIME].is_object()) {
        out_error = "SaveData: missing game_time";
        return false;
    }
    {
        const auto& time = json[KEY_GAME_TIME];
        if (!readOptionalUnsignedField(time, KEY_DAY, out.game_time.day, 1u, "game_time.day", out_error) ||
            !readOptionalFloatField(time, KEY_HOUR, out.game_time.hour, 6.0f, "game_time.hour", out_error) ||
            !readOptionalFloatField(time, KEY_MINUTE, out.game_time.minute, 0.0f, "game_time.minute", out_error) ||
            !readOptionalFloatField(time, KEY_TIME_SCALE, out.game_time.time_scale, 1.0f, "game_time.time_scale", out_error) ||
            !readOptionalBoolField(time, KEY_PAUSED, out.game_time.paused, false, "game_time.paused", out_error)) {
            return false;
        }
    }

    if (!json.contains(KEY_PLAYER) || !json[KEY_PLAYER].is_object()) {
        out_error = "SaveData: missing player";
        return false;
    }
    {
        const auto& player = json[KEY_PLAYER];
        if (!readOptionalStringField(player, KEY_MAP_NAME, out.player.map_name, "", "player.map_name", out_error)) {
            return false;
        }
        if (!player.contains(KEY_POSITION) || !readVec2f(player[KEY_POSITION], out.player.position, "player.position", out_error)) {
            if (out_error.empty()) {
                out_error = "SaveData: invalid player.position";
            }
            return false;
        }
        if (!readOptionalSignedField(player, KEY_HP, out.player.hp, 100, "player.hp", out_error) ||
            !readOptionalSignedField(player, KEY_GOLD, out.player.gold, 0, "player.gold", out_error)) {
            return false;
        }

        if (player.contains(KEY_STATE) && player[KEY_STATE].is_object()) {
            const auto& state = player[KEY_STATE];
            if (!readOptionalStringField(state, KEY_ACTION, out.player.state.action, "idle", "player.state.action", out_error) ||
                !readOptionalStringField(state, KEY_DIRECTION, out.player.state.direction, "down", "player.state.direction", out_error)) {
                return false;
            }
        } else if (player.contains(KEY_STATE)) {
            out_error = "SaveData: player.state is not an object";
            return false;
        }

        if (player.contains(KEY_INVENTORY) && player[KEY_INVENTORY].is_object()) {
            const auto& inv = player[KEY_INVENTORY];
            out.player.inventory.slots.clear();
            if (inv.contains(KEY_SLOTS) && inv[KEY_SLOTS].is_array()) {
                for (const auto& slot_json : inv[KEY_SLOTS]) {
                    ItemStackSaveData slot{};
                    if (slot_json.is_object()) {
                        if (!readOptionalUnsignedField(slot_json, KEY_ITEM_ID, slot.item_id, std::uint64_t{0}, "player.inventory.slots[].item_id", out_error) ||
                            !readOptionalSignedField(slot_json, KEY_COUNT, slot.count, 0, "player.inventory.slots[].count", out_error)) {
                            return false;
                        }
                    } else {
                        out_error = "SaveData: player.inventory.slots[] is not an object";
                        return false;
                    }
                    out.player.inventory.slots.push_back(slot);
                }
            } else if (inv.contains(KEY_SLOTS)) {
                out_error = "SaveData: player.inventory.slots is not an array";
                return false;
            }
        } else if (player.contains(KEY_INVENTORY)) {
            out_error = "SaveData: player.inventory is not an object";
            return false;
        }

        if (player.contains(KEY_HOTBAR) && player[KEY_HOTBAR].is_object()) {
            const auto& hotbar = player[KEY_HOTBAR];
            if (!readOptionalSignedField(hotbar, KEY_ACTIVE_SLOT, out.player.hotbar.active_slot, 0, "player.hotbar.active_slot", out_error)) {
                return false;
            }
            out.player.hotbar.inventory_slot_indices.clear();
            if (hotbar.contains(KEY_INVENTORY_SLOT_INDICES) && hotbar[KEY_INVENTORY_SLOT_INDICES].is_array()) {
                for (const auto& index_json : hotbar[KEY_INVENTORY_SLOT_INDICES]) {
                    int index = -1;
                    if (!readSignedValue(index_json, index, "player.hotbar.inventory_slot_indices[]", out_error)) {
                        return false;
                    }
                    out.player.hotbar.inventory_slot_indices.push_back(index);
                }
            } else if (hotbar.contains(KEY_INVENTORY_SLOT_INDICES)) {
                out_error = "SaveData: player.hotbar.inventory_slot_indices is not an array";
                return false;
            }
        } else if (player.contains(KEY_HOTBAR)) {
            out_error = "SaveData: player.hotbar is not an object";
            return false;
        }
    }

    if (json.contains(KEY_MAPS) && json[KEY_MAPS].is_array()) {
        out.maps.clear();
        for (const auto& map_json : json[KEY_MAPS]) {
            if (!map_json.is_object()) {
                continue;
            }
            MapSaveData map{};
            if (!readOptionalStringField(map_json, KEY_MAP_NAME, map.map_name, "", "maps[].map_name", out_error) ||
                !readOptionalUnsignedField(map_json, KEY_LAST_UPDATED_DAY, map.last_updated_day, 0u, "maps[].last_updated_day", out_error)) {
                return false;
            }

            if (map_json.contains(KEY_OPENED_CHESTS) && map_json[KEY_OPENED_CHESTS].is_array()) {
                for (const auto& id_json : map_json[KEY_OPENED_CHESTS]) {
                    int id = 0;
                    if (!readSignedValue(id_json, id, "maps[].opened_chests[]", out_error)) {
                        return false;
                    }
                    map.opened_chests.push_back(id);
                }
            } else if (map_json.contains(KEY_OPENED_CHESTS)) {
                out_error = "SaveData: maps[].opened_chests is not an array";
                return false;
            }

            if (map_json.contains(KEY_DEFEATED_ENCOUNTERS) && map_json[KEY_DEFEATED_ENCOUNTERS].is_array()) {
                for (const auto& id_json : map_json[KEY_DEFEATED_ENCOUNTERS]) {
                    int id = 0;
                    if (!readSignedValue(id_json, id, "maps[].defeated_encounters[]", out_error)) {
                        return false;
                    }
                    map.defeated_encounters.push_back(id);
                }
            } else if (map_json.contains(KEY_DEFEATED_ENCOUNTERS)) {
                out_error = "SaveData: maps[].defeated_encounters is not an array";
                return false;
            }

            if (map_json.contains(KEY_TILLED_TILES) && map_json[KEY_TILLED_TILES].is_array()) {
                for (const auto& tile_json : map_json[KEY_TILLED_TILES]) {
                    Vec2i tile{};
                    if (!readVec2i(tile_json, tile, "maps[].tilled_tiles[]", out_error)) {
                        return false;
                    }
                    map.tilled_tiles.push_back(tile);
                }
            } else if (map_json.contains(KEY_TILLED_TILES)) {
                out_error = "SaveData: maps[].tilled_tiles is not an array";
                return false;
            }

            if (map_json.contains(KEY_WET_TILES) && map_json[KEY_WET_TILES].is_array()) {
                for (const auto& tile_json : map_json[KEY_WET_TILES]) {
                    Vec2i tile{};
                    if (!readVec2i(tile_json, tile, "maps[].wet_tiles[]", out_error)) {
                        return false;
                    }
                    map.wet_tiles.push_back(tile);
                }
            } else if (map_json.contains(KEY_WET_TILES)) {
                out_error = "SaveData: maps[].wet_tiles is not an array";
                return false;
            }

            if (map_json.contains(KEY_CROPS) && map_json[KEY_CROPS].is_array()) {
                for (const auto& crop_json : map_json[KEY_CROPS]) {
                    if (!crop_json.is_object()) {
                        continue;
                    }
                    CropSaveData crop{};
                    if (crop_json.contains(KEY_TILE) && crop_json[KEY_TILE].is_object()) {
                        if (!readVec2i(crop_json[KEY_TILE], crop.tile, "maps[].crops[].tile", out_error)) {
                            return false;
                        }
                    } else if (crop_json.contains(KEY_TILE)) {
                        out_error = "SaveData: maps[].crops[].tile is not an object";
                        return false;
                    }
                    if (!readOptionalStringField(crop_json, KEY_CROP_TYPE, crop.crop_type, "unknown", "maps[].crops[].crop_type", out_error) ||
                        !readOptionalStringField(crop_json, KEY_GROWTH_STAGE, crop.growth_stage, "seed", "maps[].crops[].growth_stage", out_error) ||
                        !readOptionalUnsignedField(crop_json, KEY_PLANTED_DAY, crop.planted_day, 0u, "maps[].crops[].planted_day", out_error) ||
                        !readOptionalSignedField(crop_json, KEY_STAGE_COUNTDOWN, crop.stage_countdown, 0, "maps[].crops[].stage_countdown", out_error)) {
                        return false;
                    }
                    map.crops.push_back(std::move(crop));
                }
            } else if (map_json.contains(KEY_CROPS)) {
                out_error = "SaveData: maps[].crops is not an array";
                return false;
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
                        if (!readVec2i(node_json[KEY_TILE], node.tile, "maps[].resource_nodes[].tile", out_error)) {
                            return false;
                        }
                    } else if (node_json.contains(KEY_TILE)) {
                        out_error = "SaveData: maps[].resource_nodes[].tile is not an object";
                        return false;
                    }
                    if (!readOptionalStringField(node_json, KEY_NODE_TYPE, node.node_type, "unknown", "maps[].resource_nodes[].node_type", out_error) ||
                        !readOptionalSignedField(node_json, KEY_HIT_COUNT, node.hit_count, 0, "maps[].resource_nodes[].hit_count", out_error) ||
                        !readOptionalSignedField(node_json, KEY_HITS_TO_BREAK, node.hits_to_break, 0, "maps[].resource_nodes[].hits_to_break", out_error) ||
                        !readOptionalUnsignedField(node_json, KEY_DROP_ITEM_ID, node.drop_item_id, std::uint64_t{0}, "maps[].resource_nodes[].drop_item_id", out_error)) {
                        return false;
                    }
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
        if (!readOptionalStringField(appearance, KEY_PROFILE_ID, out.appearance_state.profile_id, "player_default", "appearance_state.profile_id", out_error) ||
            !readOptionalStringField(appearance, KEY_GENDER, out.appearance_state.gender, "male", "appearance_state.gender", out_error)) {
            return false;
        }
        out.appearance_state.slots.clear();
        if (appearance.contains(KEY_SLOTS)) {
            if (!appearance[KEY_SLOTS].is_object()) {
                out_error = "SaveData: appearance_state.slots is not an object";
                return false;
            }
            for (const auto& [slot, variant] : appearance[KEY_SLOTS].items()) {
                std::string variant_id;
                if (!readStringValue(variant, variant_id, "appearance_state.slots." + slot, out_error)) {
                    return false;
                }
                out.appearance_state.slots.emplace(slot, std::move(variant_id));
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
            std::uint64_t max_active_members = 0;
            if (!readUnsignedValue(party_state[KEY_MAX_ACTIVE_MEMBERS], max_active_members, "party_state.max_active_members", out_error)) {
                return false;
            }
            out.party_state.max_active_members = max_active_members == 0U ? 1U : static_cast<std::size_t>(max_active_members);
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
        if (!readOptionalBoolField(combat_state, KEY_PENDING_BATTLE, out.combat_state.pending_battle, false, "combat_state.pending_battle", out_error) ||
            !readOptionalStringField(combat_state, KEY_TROOP_ID, out.combat_state.troop_id, "", "combat_state.troop_id", out_error) ||
            !readOptionalUnsignedField(combat_state, KEY_ESCAPE_ATTEMPT_COUNT, out.combat_state.escape_attempt_count, 0u, "combat_state.escape_attempt_count", out_error)) {
            return false;
        }
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
