#include "save_migrator.h"

#include "save_data.h"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <string_view>

namespace game::save {
namespace {

constexpr std::string_view KEY_SCHEMA_VERSION = json_keys::SCHEMA_VERSION;
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
constexpr std::string_view KEY_LEVEL = json_keys::LEVEL;
constexpr std::string_view KEY_TOTAL_EXP = json_keys::TOTAL_EXP;
constexpr std::string_view KEY_ITEM_STOCKS = json_keys::ITEM_STOCKS;
constexpr std::string_view KEY_ESCAPE_ATTEMPT_COUNT = json_keys::ESCAPE_ATTEMPT_COUNT;
constexpr std::string_view KEY_PROFILE_ID = json_keys::PROFILE_ID;
constexpr std::string_view KEY_GENDER = "gender";
constexpr std::string_view KEY_SLOTS = "slots";
constexpr std::uint32_t SCHEMA_VERSION_SCRIPT_STATE = 7u;
constexpr std::uint32_t SCHEMA_VERSION_PARTY_MAX_ACTIVE_MEMBERS = 8u;
constexpr std::uint32_t DEFAULT_MAX_ACTIVE_MEMBERS =
    static_cast<std::uint32_t>(game::defs::kDefaultMaxActivePartyMembers);

bool ensureObjectField(nlohmann::json& json, std::string_view key, std::string& out_error) {
    if (!json.contains(key)) {
        json[key] = nlohmann::json::object();
        return true;
    }
    if (!json[key].is_object()) {
        out_error = "SaveMigrator: field '" + std::string(key) + "' is not an object";
        return false;
    }
    return true;
}

bool ensureArrayField(nlohmann::json& json, std::string_view key, std::string& out_error) {
    if (!json.contains(key)) {
        json[key] = nlohmann::json::array();
        return true;
    }
    if (!json[key].is_array()) {
        out_error = "SaveMigrator: field '" + std::string(key) + "' is not an array";
        return false;
    }
    return true;
}

bool ensureUIntField(nlohmann::json& json, std::string_view key, std::uint32_t default_value, std::string& out_error) {
    if (!json.contains(key)) {
        json[key] = default_value;
        return true;
    }
    if (!json[key].is_number_unsigned()) {
        out_error = "SaveMigrator: field '" + std::string(key) + "' is not an unsigned int";
        return false;
    }
    return true;
}

bool ensureBoolField(nlohmann::json& json, std::string_view key, bool default_value, std::string& out_error) {
    if (!json.contains(key)) {
        json[key] = default_value;
        return true;
    }
    if (!json[key].is_boolean()) {
        out_error = "SaveMigrator: field '" + std::string(key) + "' is not a boolean";
        return false;
    }
    return true;
}

bool ensureStringField(nlohmann::json& json, std::string_view key, std::string_view default_value, std::string& out_error) {
    if (!json.contains(key)) {
        json[key] = default_value;
        return true;
    }
    if (!json[key].is_string()) {
        out_error = "SaveMigrator: field '" + std::string(key) + "' is not a string";
        return false;
    }
    return true;
}

bool normalizeQuestState(nlohmann::json& quest_state, std::string& out_error) {
    if (!ensureArrayField(quest_state, KEY_ACTIVE_QUESTS, out_error)) {
        return false;
    }
    if (!ensureArrayField(quest_state, KEY_COMPLETED_QUESTS, out_error)) {
        return false;
    }
    if (!ensureObjectField(quest_state, KEY_OBJECTIVE_PROGRESS, out_error)) {
        return false;
    }
    return true;
}

bool normalizeSkillState(nlohmann::json& skill_state, std::string& out_error) {
    if (!ensureArrayField(skill_state, KEY_LEARNED_SKILLS, out_error)) {
        return false;
    }
    if (!ensureObjectField(skill_state, KEY_SKILL_LEVELS, out_error)) {
        return false;
    }
    if (!ensureObjectField(skill_state, KEY_SKILL_COOLDOWNS, out_error)) {
        return false;
    }
    return true;
}

bool normalizeCombatState(nlohmann::json& combat_state, std::string& out_error) {
    if (!ensureBoolField(combat_state, KEY_PENDING_BATTLE, false, out_error)) {
        return false;
    }
    if (!ensureStringField(combat_state, KEY_TROOP_ID, "", out_error)) {
        return false;
    }
    if (!ensureArrayField(combat_state, KEY_ACTOR_IDS, out_error)) {
        return false;
    }
    if (!ensureObjectField(combat_state, KEY_ITEM_STOCKS, out_error)) {
        return false;
    }
    if (!ensureUIntField(combat_state, KEY_ESCAPE_ATTEMPT_COUNT, 0u, out_error)) {
        return false;
    }
    return true;
}

bool normalizePartyState(nlohmann::json& party_state, std::string& out_error) {
    if (!ensureArrayField(party_state, KEY_RECRUITED_ACTOR_IDS, out_error)) {
        return false;
    }
    if (!ensureArrayField(party_state, KEY_ACTIVE_ACTOR_IDS, out_error)) {
        return false;
    }
    if (!ensureUIntField(party_state, KEY_MAX_ACTIVE_MEMBERS, DEFAULT_MAX_ACTIVE_MEMBERS, out_error)) {
        return false;
    }
    if (party_state[KEY_MAX_ACTIVE_MEMBERS].get<std::uint32_t>() == 0u) {
        party_state[KEY_MAX_ACTIVE_MEMBERS] = 1u;
    }
    return true;
}

bool normalizeEquipmentState(nlohmann::json& equipment_state, std::string& out_error) {
    if (!ensureObjectField(equipment_state, KEY_LOADOUTS, out_error)) {
        return false;
    }
    return true;
}

bool normalizePartyRuntimeState(nlohmann::json& runtime_state, std::string& out_error) {
    if (!ensureObjectField(runtime_state, KEY_ACTOR_STATES, out_error)) {
        return false;
    }
    for (auto& [actor_id, actor_state] : runtime_state[KEY_ACTOR_STATES].items()) {
        (void)actor_id;
        if (!actor_state.is_object()) {
            out_error = "SaveMigrator: party_runtime_state.actor_states entry is not an object";
            return false;
        }
        if (!actor_state.contains(KEY_LEVEL)) {
            actor_state[KEY_LEVEL] = 1;
        } else if (!actor_state[KEY_LEVEL].is_number_integer()) {
            out_error = "SaveMigrator: party runtime level is not an int";
            return false;
        }
        if (!actor_state.contains(KEY_TOTAL_EXP)) {
            actor_state[KEY_TOTAL_EXP] = 0;
        } else if (!actor_state[KEY_TOTAL_EXP].is_number_integer()) {
            out_error = "SaveMigrator: party runtime total_exp is not an int";
            return false;
        }
    }
    return true;
}

bool normalizeAppearanceState(nlohmann::json& appearance_state, std::string& out_error) {
    if (!ensureStringField(appearance_state, KEY_PROFILE_ID, "player_default", out_error)) {
        return false;
    }
    if (!ensureStringField(appearance_state, KEY_GENDER, "male", out_error)) {
        return false;
    }
    if (!ensureObjectField(appearance_state, KEY_SLOTS, out_error)) {
        return false;
    }
    return true;
}

bool normalizeScriptState(nlohmann::json& script_state, std::string& out_error) {
    if (!script_state.is_object()) {
        out_error = "SaveMigrator: field '" + std::string(KEY_SCRIPT_STATE) + "' is not an object";
        return false;
    }
    for (const auto& [key, value] : script_state.items()) {
        if (!value.is_null() && !value.is_boolean() && !value.is_number() && !value.is_string()) {
            out_error = "SaveMigrator: script_state." + key + " is not a JSON primitive";
            return false;
        }
    }
    return true;
}

bool normalizeLatestFields(nlohmann::json& json, std::string& out_error) {
    if (!ensureObjectField(json, KEY_QUEST_STATE, out_error)) {
        return false;
    }
    if (!ensureObjectField(json, KEY_SKILL_STATE, out_error)) {
        return false;
    }
    if (!ensureObjectField(json, KEY_APPEARANCE_STATE, out_error)) {
        return false;
    }
    if (!ensureObjectField(json, KEY_COMBAT_STATE, out_error)) {
        return false;
    }
    if (!ensureObjectField(json, KEY_PARTY_STATE, out_error)) {
        return false;
    }
    if (!ensureObjectField(json, KEY_EQUIPMENT_STATE, out_error)) {
        return false;
    }
    if (!ensureObjectField(json, KEY_PARTY_RUNTIME_STATE, out_error)) {
        return false;
    }
    if (!ensureObjectField(json, KEY_SCRIPT_STATE, out_error)) {
        return false;
    }
    if (!normalizeQuestState(json[KEY_QUEST_STATE], out_error)) {
        return false;
    }
    if (!normalizeSkillState(json[KEY_SKILL_STATE], out_error)) {
        return false;
    }
    if (!normalizeAppearanceState(json[KEY_APPEARANCE_STATE], out_error)) {
        return false;
    }
    if (!normalizeCombatState(json[KEY_COMBAT_STATE], out_error)) {
        return false;
    }
    if (!normalizePartyState(json[KEY_PARTY_STATE], out_error)) {
        return false;
    }
    if (!normalizeEquipmentState(json[KEY_EQUIPMENT_STATE], out_error)) {
        return false;
    }
    if (!normalizePartyRuntimeState(json[KEY_PARTY_RUNTIME_STATE], out_error)) {
        return false;
    }
    if (!normalizeScriptState(json[KEY_SCRIPT_STATE], out_error)) {
        return false;
    }
    return true;
}

bool migrateV4ToV5(nlohmann::json& json, std::string& out_error) {
    if (!ensureObjectField(json, KEY_APPEARANCE_STATE, out_error)) {
        return false;
    }
    if (!normalizeAppearanceState(json[KEY_APPEARANCE_STATE], out_error)) {
        return false;
    }
    json[KEY_SCHEMA_VERSION] = 5u;
    return true;
}

bool migrateV5ToV6(nlohmann::json& json, std::string& out_error) {
    if (!ensureObjectField(json, KEY_PARTY_RUNTIME_STATE, out_error)) {
        return false;
    }
    if (!normalizePartyRuntimeState(json[KEY_PARTY_RUNTIME_STATE], out_error)) {
        return false;
    }
    json[KEY_SCHEMA_VERSION] = 6u;
    return true;
}

bool migrateV6ToV7(nlohmann::json& json, std::string& out_error) {
    if (!ensureObjectField(json, KEY_SCRIPT_STATE, out_error)) {
        return false;
    }
    if (!normalizeScriptState(json[KEY_SCRIPT_STATE], out_error)) {
        return false;
    }
    json[KEY_SCHEMA_VERSION] = SCHEMA_VERSION_SCRIPT_STATE;
    return true;
}

bool migrateV7ToV8(nlohmann::json& json, std::string& out_error) {
    if (!ensureObjectField(json, KEY_PARTY_STATE, out_error)) {
        return false;
    }
    if (!normalizePartyState(json[KEY_PARTY_STATE], out_error)) {
        return false;
    }
    json[KEY_SCHEMA_VERSION] = SCHEMA_VERSION_PARTY_MAX_ACTIVE_MEMBERS;
    return true;
}

} // namespace

bool migrateToLatest(nlohmann::json& json, std::string& out_error) {
    out_error.clear();

    if (!json.is_object()) {
        out_error = "SaveMigrator: root is not an object";
        return false;
    }

    const std::uint32_t schema_version = json.value<std::uint32_t>(KEY_SCHEMA_VERSION.data(), 0u);
    if (schema_version == 0u) {
        out_error = "SaveMigrator: missing schema_version";
        return false;
    }
    if (schema_version > SAVE_SCHEMA_VERSION) {
        out_error = "SaveMigrator: unsupported schema_version";
        return false;
    }

    if (schema_version >= 2u && schema_version < 4u) {
        if (!normalizeLatestFields(json, out_error)) {
            return false;
        }
        json[KEY_SCHEMA_VERSION] = 4u;
    }

    if (json[KEY_SCHEMA_VERSION] == 4u) {
        if (!migrateV4ToV5(json, out_error)) {
            return false;
        }
    }

    if (json[KEY_SCHEMA_VERSION] == 5u) {
        if (!migrateV5ToV6(json, out_error)) {
            return false;
        }
    }

    if (json[KEY_SCHEMA_VERSION] == 6u) {
        if (!migrateV6ToV7(json, out_error)) {
            return false;
        }
    }

    if (json[KEY_SCHEMA_VERSION] == SCHEMA_VERSION_SCRIPT_STATE) {
        if (!migrateV7ToV8(json, out_error)) {
            return false;
        }
    }

    if (json[KEY_SCHEMA_VERSION] == SAVE_SCHEMA_VERSION) {
        return normalizeLatestFields(json, out_error);
    }

    out_error = "SaveMigrator: unsupported schema_version";
    return false;
}

} // namespace game::save
