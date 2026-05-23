#include "game/data/rpg_catalog.h"

#include "engine/utils/json_file_loader.h"
#include "game/data/battle_background_id.h"
#include "game/data/item_catalog.h"
#include "game/data/rpg_catalog_loaders.h"

#include <entt/core/hashed_string.hpp>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <string>
#include <string_view>

namespace game::data {
namespace {

using Json = nlohmann::json;

} // namespace

entt::id_type RpgCatalog::hashId(const std::string_view id) {
    return entt::hashed_string{id.data(), id.size()}.value();
}

bool RpgCatalog::loadManifest(const std::string_view file_path) {
    Json root{};
    if (!engine::utils::loadJsonObjectFile(file_path, root, "RpgCatalogManifest", spdlog::level::err)) {
        return false;
    }

    const auto schema_version = root.value("schema_version", 0U);
    if (schema_version == 0U) {
        spdlog::error("RpgCatalog: manifest '{}' 缺少有效 schema_version", file_path);
        return false;
    }

    RpgManifest parsed{};
    parsed.schema_version = schema_version;

    if (const auto versions_it = root.find("content_versions"); versions_it != root.end()) {
        if (!versions_it->is_object()) {
            spdlog::error("RpgCatalog: manifest '{}' content_versions 必须是 object", file_path);
            return false;
        }
        for (const auto& [module, version] : versions_it->items()) {
            if (!version.is_number_unsigned()) {
                spdlog::error("RpgCatalog: manifest '{}' content_versions.{} 必须是 unsigned int", file_path, module);
                return false;
            }
            parsed.content_versions_[module] = version.get<std::uint32_t>();
        }
    }

    if (const auto features_it = root.find("features"); features_it != root.end()) {
        if (!features_it->is_object()) {
            spdlog::error("RpgCatalog: manifest '{}' features 必须是 object", file_path);
            return false;
        }
        for (const auto& [feature, enabled] : features_it->items()) {
            if (!enabled.is_boolean()) {
                spdlog::error("RpgCatalog: manifest '{}' features.{} 必须是 boolean", file_path, feature);
                return false;
            }
            parsed.features_[feature] = enabled.get<bool>();
        }
    }

    if (const auto files_it = root.find("files"); files_it != root.end()) {
        if (!files_it->is_object()) {
            spdlog::error("RpgCatalog: manifest '{}' files 必须是 object", file_path);
            return false;
        }
        for (const auto& [module, rel_path] : files_it->items()) {
            if (!rel_path.is_string()) {
                spdlog::error("RpgCatalog: manifest '{}' files.{} 必须是 string", file_path, module);
                return false;
            }
            parsed.files_[module] = rel_path.get<std::string>();
        }
    }

    manifest_ = std::move(parsed);
    has_manifest_ = true;
    return true;
}

bool RpgCatalog::loadClasses(const std::string_view file_path) {
    return loadRpgClassesFile(file_path, classes_);
}

bool RpgCatalog::loadActors(const std::string_view file_path) {
    return loadRpgActorsFile(file_path, actors_);
}

bool RpgCatalog::loadSkills(const std::string_view file_path) {
    return loadRpgSkillsFile(file_path, skills_);
}

bool RpgCatalog::loadStates(const std::string_view file_path) {
    return loadRpgStatesFile(file_path, states_);
}

bool RpgCatalog::loadEquipment(const std::string_view file_path) {
    return loadRpgEquipmentFile(file_path, equipment_);
}

bool RpgCatalog::loadEnemies(const std::string_view file_path) {
    return loadRpgEnemiesFile(file_path, enemies_);
}

bool RpgCatalog::loadTroops(const std::string_view file_path) {
    return loadRpgTroopsFile(file_path, troops_);
}

bool RpgCatalog::validateReferences(std::string& out_error, const ItemCatalog* item_catalog) const {
    for (const auto& [enemy_id, enemy] : enemies_) {
        (void)enemy_id;
        for (const auto& action : enemy.actions_) {
            const entt::id_type skill_id_hash = RpgCatalog::hashId(action.skill_id_);
            if (!skills_.contains(skill_id_hash)) {
                out_error = "Enemy '" + enemy.id_ + "' references missing skill '" + action.skill_id_ + "'";
                return false;
            }
        }

        if (item_catalog) {
            for (const auto& drop : enemy.drops_) {
                const entt::id_type item_id_hash = RpgCatalog::hashId(drop.item_id_);
                if (!item_catalog->hasItem(item_id_hash)) {
                    out_error = "Enemy '" + enemy.id_ + "' references missing item '" + drop.item_id_ + "'";
                    return false;
                }
            }
        }
    }

    for (const auto& [troop_id, troop] : troops_) {
        (void)troop_id;
        if (!troop.battle_background_id_.empty() &&
            !game::data::isValidBattleBackgroundId(troop.battle_background_id_)) {
            out_error = "Troop '" + troop.id_ + "' has invalid battle_background_id '" +
                        troop.battle_background_id_ + "'";
            return false;
        }

        for (const auto& member : troop.members_) {
            const entt::id_type enemy_id_hash = RpgCatalog::hashId(member.enemy_id_);
            if (!enemies_.contains(enemy_id_hash)) {
                out_error = "Troop '" + troop.id_ + "' references missing enemy '" + member.enemy_id_ + "'";
                return false;
            }
        }
    }

    for (const auto& [actor_id, actor] : actors_) {
        (void)actor_id;
        const entt::id_type class_id_hash = RpgCatalog::hashId(actor.class_id_);
        if (!classes_.contains(class_id_hash)) {
            out_error = "Actor '" + actor.id_ + "' references missing class '" + actor.class_id_ + "'";
            return false;
        }

        for (const auto& skill_id : actor.skill_ids_) {
            const entt::id_type skill_id_hash = RpgCatalog::hashId(skill_id);
            if (!skills_.contains(skill_id_hash)) {
                out_error = "Actor '" + actor.id_ + "' references missing skill '" + skill_id + "'";
                return false;
            }
        }
    }

    for (const auto& [skill_id, skill] : skills_) {
        (void)skill_id;
        for (const auto& effect : skill.effects_) {
            if (effect.type == EffectType::AddState || effect.type == EffectType::RemoveState) {
                if (effect.target_id.empty()) {
                    out_error = "Skill '" + skill.id_ + "' has state effect without target_id";
                    return false;
                }

                const entt::id_type state_id_hash = RpgCatalog::hashId(effect.target_id);
                if (!states_.contains(state_id_hash)) {
                    out_error = "Skill '" + skill.id_ + "' references missing state '" + effect.target_id + "'";
                    return false;
                }
            }
        }
    }

    for (const auto& [item_id, equipment] : equipment_) {
        (void)item_id;
        if (item_catalog) {
            const auto* item = item_catalog->findItem(equipment.item_id_hash_);
            if (!item) {
                out_error = "Equipment '" + equipment.item_id_ + "' references missing item";
                return false;
            }
            if (item->category_ != ItemCategory::Equipment) {
                out_error = "Equipment '" + equipment.item_id_ + "' item category is not equipment";
                return false;
            }
            if (item->stack_limit_ != 1) {
                out_error = "Equipment '" + equipment.item_id_ + "' item stack_limit must be 1";
                return false;
            }
            if (item->on_use_.has_value() || item->battle_use_.has_value()) {
                out_error = "Equipment '" + equipment.item_id_ + "' item must not define on_use or battle_use";
                return false;
            }
        }

        if (equipment.slot_ == EquipmentSlotId::Unknown) {
            out_error = "Equipment '" + equipment.item_id_ + "' has invalid slot";
            return false;
        }

        for (const auto& class_id : equipment.allowed_classes_) {
            if (!classes_.contains(RpgCatalog::hashId(class_id))) {
                out_error = "Equipment '" + equipment.item_id_ + "' references missing class '" + class_id + "'";
                return false;
            }
        }
        for (const auto& actor_id : equipment.allowed_actors_) {
            if (!actors_.contains(RpgCatalog::hashId(actor_id))) {
                out_error = "Equipment '" + equipment.item_id_ + "' references missing actor '" + actor_id + "'";
                return false;
            }
        }
    }

    // TODO: 当 trait target 收敛为强类型 ID 后，补充 TraitData::target 的语义校验。

    out_error.clear();
    return true;
}

const ClassData* RpgCatalog::findClass(const entt::id_type id_hash) const {
    if (const auto it = classes_.find(id_hash); it != classes_.end()) {
        return &it->second;
    }
    return nullptr;
}

const ClassData* RpgCatalog::findClass(const std::string_view id) const {
    return findClass(RpgCatalog::hashId(id));
}

const ActorData* RpgCatalog::findActor(const entt::id_type id_hash) const {
    if (const auto it = actors_.find(id_hash); it != actors_.end()) {
        return &it->second;
    }
    return nullptr;
}

const ActorData* RpgCatalog::findActor(const std::string_view id) const {
    return findActor(RpgCatalog::hashId(id));
}

const SkillData* RpgCatalog::findSkill(const entt::id_type id_hash) const {
    if (const auto it = skills_.find(id_hash); it != skills_.end()) {
        return &it->second;
    }
    return nullptr;
}

const SkillData* RpgCatalog::findSkill(const std::string_view id) const {
    return findSkill(RpgCatalog::hashId(id));
}

const StateData* RpgCatalog::findState(const entt::id_type id_hash) const {
    if (const auto it = states_.find(id_hash); it != states_.end()) {
        return &it->second;
    }
    return nullptr;
}

const StateData* RpgCatalog::findState(const std::string_view id) const {
    return findState(RpgCatalog::hashId(id));
}

const EquipmentData* RpgCatalog::findEquipmentByItem(const entt::id_type item_id_hash) const {
    if (const auto it = equipment_.find(item_id_hash); it != equipment_.end()) {
        return &it->second;
    }
    return nullptr;
}

const EquipmentData* RpgCatalog::findEquipmentByItem(const std::string_view item_id) const {
    return findEquipmentByItem(RpgCatalog::hashId(item_id));
}

const EnemyData* RpgCatalog::findEnemy(const entt::id_type id_hash) const {
    if (const auto it = enemies_.find(id_hash); it != enemies_.end()) {
        return &it->second;
    }
    return nullptr;
}

const EnemyData* RpgCatalog::findEnemy(const std::string_view id) const {
    return findEnemy(RpgCatalog::hashId(id));
}

const TroopData* RpgCatalog::findTroop(const entt::id_type id_hash) const {
    if (const auto it = troops_.find(id_hash); it != troops_.end()) {
        return &it->second;
    }
    return nullptr;
}

const TroopData* RpgCatalog::findTroop(const std::string_view id) const {
    return findTroop(RpgCatalog::hashId(id));
}

std::vector<const ActorData*> RpgCatalog::listActors() const {
    std::vector<const ActorData*> result{};
    result.reserve(actors_.size());
    for (const auto& [id, actor] : actors_) {
        (void)id;
        result.push_back(&actor);
    }
    std::sort(result.begin(), result.end(), [](const ActorData* lhs, const ActorData* rhs) {
        return lhs->id_ < rhs->id_;
    });
    return result;
}

std::vector<const EquipmentData*> RpgCatalog::listEquipment() const {
    std::vector<const EquipmentData*> result{};
    result.reserve(equipment_.size());
    for (const auto& [id, equipment] : equipment_) {
        (void)id;
        result.push_back(&equipment);
    }
    std::sort(result.begin(), result.end(), [](const EquipmentData* lhs, const EquipmentData* rhs) {
        return lhs->item_id_ < rhs->item_id_;
    });
    return result;
}

std::vector<const TroopData*> RpgCatalog::listTroops() const {
    std::vector<const TroopData*> result{};
    result.reserve(troops_.size());
    for (const auto& [id, troop] : troops_) {
        (void)id;
        result.push_back(&troop);
    }
    std::sort(result.begin(), result.end(), [](const TroopData* lhs, const TroopData* rhs) {
        return lhs->id_ < rhs->id_;
    });
    return result;
}

} // namespace game::data
