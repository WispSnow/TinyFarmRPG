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
constexpr std::string_view KEY_COMBAT_STATE = json_keys::COMBAT_STATE;
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
constexpr std::string_view KEY_ITEM_STOCKS = json_keys::ITEM_STOCKS;
constexpr std::string_view KEY_ESCAPE_ATTEMPT_COUNT = json_keys::ESCAPE_ATTEMPT_COUNT;

bool ensureObjectField(nlohmann::json& json, std::string_view key, std::string& out_error) {
    if (!json.contains(key)) {
        json[key] = nlohmann::json::object();
        return true;
    }
    if (!json[key].is_object()) {
        out_error = "SaveMigrator: 字段 '" + std::string(key) + "' 不是 object";
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
        out_error = "SaveMigrator: 字段 '" + std::string(key) + "' 不是 array";
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
        out_error = "SaveMigrator: 字段 '" + std::string(key) + "' 不是 unsigned int";
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
        out_error = "SaveMigrator: 字段 '" + std::string(key) + "' 不是 boolean";
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
        out_error = "SaveMigrator: 字段 '" + std::string(key) + "' 不是 string";
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
    return true;
}

bool migrateV2ToV3(nlohmann::json& json, std::string& out_error) {
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
    if (!normalizeQuestState(json[KEY_QUEST_STATE], out_error)) {
        return false;
    }
    if (!normalizeSkillState(json[KEY_SKILL_STATE], out_error)) {
        return false;
    }
    if (!normalizeCombatState(json[KEY_COMBAT_STATE], out_error)) {
        return false;
    }
    if (!normalizePartyState(json[KEY_PARTY_STATE], out_error)) {
        return false;
    }

    json[KEY_SCHEMA_VERSION] = SAVE_SCHEMA_VERSION;
    return true;
}

bool normalizeV3(nlohmann::json& json, std::string& out_error) {
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
    if (!normalizeQuestState(json[KEY_QUEST_STATE], out_error)) {
        return false;
    }
    if (!normalizeSkillState(json[KEY_SKILL_STATE], out_error)) {
        return false;
    }
    if (!normalizeCombatState(json[KEY_COMBAT_STATE], out_error)) {
        return false;
    }
    if (!normalizePartyState(json[KEY_PARTY_STATE], out_error)) {
        return false;
    }

    json[KEY_SCHEMA_VERSION] = SAVE_SCHEMA_VERSION;
    return true;
}

} // namespace

bool migrateToLatest(nlohmann::json& json, std::string& out_error) {
    out_error.clear();

    if (!json.is_object()) {
        out_error = "SaveMigrator: 根节点不是 object";
        return false;
    }

    const std::uint32_t schema_version = json.value<std::uint32_t>(KEY_SCHEMA_VERSION.data(), 0u);
    if (schema_version == 0u) {
        out_error = "SaveMigrator: 缺少 schema_version";
        return false;
    }
    if (schema_version > SAVE_SCHEMA_VERSION) {
        out_error = "SaveMigrator: schema_version 不支持";
        return false;
    }

    if (schema_version == SAVE_SCHEMA_VERSION) {
        return normalizeV3(json, out_error);
    }
    if (schema_version == 2u) {
        return migrateV2ToV3(json, out_error);
    }

    out_error = "SaveMigrator: schema_version 不支持";
    return false;
}

} // namespace game::save
