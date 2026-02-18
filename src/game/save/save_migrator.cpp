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
constexpr std::string_view KEY_COMBAT_STATE = json_keys::COMBAT_STATE;

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
