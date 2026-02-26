#include "game/data/rpg_catalog.h"

#include "engine/utils/json_file_loader.h"

#include <entt/core/hashed_string.hpp>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <string>
#include <string_view>

namespace game::data {
namespace {

using Json = nlohmann::json;

[[nodiscard]] bool isValidRate(const int value) {
    return value >= 0 && value <= 100;
}

[[nodiscard]] bool isValidChance(const float value) {
    return value >= 0.0F && value <= 1.0F;
}

[[nodiscard]] bool parseParamArray(const Json& node, ParamArray& out_params) {
    out_params.fill(0);
    if (node.is_array()) {
        if (node.size() != kParamCount) {
            return false;
        }
        for (std::size_t i = 0; i < kParamCount; ++i) {
            if (!node[i].is_number_integer()) {
                return false;
            }
            out_params[i] = node[i].get<int>();
        }
        return true;
    }

    if (!node.is_object()) {
        return false;
    }

    for (std::size_t i = 0; i < kParamCount; ++i) {
        const auto index = static_cast<ParamIndex>(i);
        const auto key = std::string(toString(index));
        const auto it = node.find(key);
        if (it == node.end() || !it->is_number_integer()) {
            return false;
        }
        out_params[i] = it->get<int>();
    }
    return true;
}

[[nodiscard]] bool parseTraitList(const Json& node, std::vector<TraitData>& out_traits) {
    out_traits.clear();
    if (node.is_null()) {
        return true;
    }
    if (!node.is_array()) {
        return false;
    }

    for (const auto& trait_node : node) {
        if (!trait_node.is_object()) {
            return false;
        }

        TraitData trait{};
        const auto type = traitTypeFromString(trait_node.value("type", std::string{}));
        if (!type.has_value()) {
            return false;
        }
        trait.type = *type;
        trait.target = trait_node.value("target", std::string{});
        trait.value = trait_node.value("value", 0.0F);
        out_traits.push_back(std::move(trait));
    }
    return true;
}

[[nodiscard]] bool parseEffectList(const Json& node, std::vector<EffectData>& out_effects) {
    out_effects.clear();
    if (node.is_null()) {
        return true;
    }
    if (!node.is_array()) {
        return false;
    }

    for (const auto& effect_node : node) {
        if (!effect_node.is_object()) {
            return false;
        }

        EffectData effect{};
        const auto type = effectTypeFromString(effect_node.value("type", std::string{}));
        if (!type.has_value()) {
            return false;
        }

        effect.type = *type;
        effect.target_id = effect_node.value("target_id", std::string{});
        effect.value1 = effect_node.value("value1", 0.0F);
        effect.value2 = effect_node.value("value2", 0.0F);
        effect.count = effect_node.value("count", 0);
        out_effects.push_back(std::move(effect));
    }
    return true;
}

[[nodiscard]] bool parseDamageData(const Json& node, DamageFormulaData& out_damage) {
    if (!node.is_object()) {
        return false;
    }

    const auto damage_type = damageTypeFromString(node.value("type", std::string{"none"}));
    if (!damage_type.has_value()) {
        return false;
    }

    out_damage.type = *damage_type;
    out_damage.formula = node.value("formula", std::string{});
    out_damage.variance = node.value("variance", 0);
    out_damage.critical = node.value("critical", false);
    return true;
}

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
    Json root{};
    if (!engine::utils::loadJsonObjectFile(file_path, root, "RpgCatalogClasses", spdlog::level::err)) {
        return false;
    }

    const auto classes_it = root.find("classes");
    if (classes_it == root.end() || !classes_it->is_array()) {
        spdlog::error("RpgCatalog: classes 文件 '{}' 缺少 classes 数组", file_path);
        return false;
    }

    classes_.clear();
    for (const auto& class_node : *classes_it) {
        if (!class_node.is_object()) {
            spdlog::error("RpgCatalog: classes 文件 '{}' 存在非 object 条目", file_path);
            return false;
        }

        ClassData klass{};
        klass.id_ = class_node.value("id", std::string{});
        if (klass.id_.empty()) {
            spdlog::error("RpgCatalog: classes 文件 '{}' 存在空 id 条目", file_path);
            return false;
        }
        klass.id_hash_ = RpgCatalog::hashId(klass.id_);
        klass.display_name_ = class_node.value("display_name", klass.id_);

        const auto params_it = class_node.find("base_params");
        if (params_it == class_node.end() || !parseParamArray(*params_it, klass.base_params_)) {
            spdlog::error("RpgCatalog: class '{}' base_params 配置非法", klass.id_);
            return false;
        }

        if (classes_.contains(klass.id_hash_)) {
            spdlog::error("RpgCatalog: classes 文件 '{}' 存在重复 id '{}'", file_path, klass.id_);
            return false;
        }
        classes_.insert_or_assign(klass.id_hash_, std::move(klass));
    }

    return true;
}

bool RpgCatalog::loadActors(const std::string_view file_path) {
    Json root{};
    if (!engine::utils::loadJsonObjectFile(file_path, root, "RpgCatalogActors", spdlog::level::err)) {
        return false;
    }

    const auto actors_it = root.find("actors");
    if (actors_it == root.end() || !actors_it->is_array()) {
        spdlog::error("RpgCatalog: actors 文件 '{}' 缺少 actors 数组", file_path);
        return false;
    }

    actors_.clear();
    for (const auto& actor_node : *actors_it) {
        if (!actor_node.is_object()) {
            spdlog::error("RpgCatalog: actors 文件 '{}' 存在非 object 条目", file_path);
            return false;
        }

        ActorData actor{};
        actor.id_ = actor_node.value("id", std::string{});
        if (actor.id_.empty()) {
            spdlog::error("RpgCatalog: actors 文件 '{}' 存在空 id 条目", file_path);
            return false;
        }
        actor.id_hash_ = RpgCatalog::hashId(actor.id_);
        actor.display_name_ = actor_node.value("display_name", actor.id_);
        actor.class_id_ = actor_node.value("class_id", std::string{});
        actor.initial_level_ = std::max(1, actor_node.value("initial_level", 1));
        actor.max_level_ = std::max(actor.initial_level_, actor_node.value("max_level", actor.initial_level_));

        if (actor.class_id_.empty()) {
            spdlog::error("RpgCatalog: actor '{}' 缺少 class_id", actor.id_);
            return false;
        }

        if (actors_.contains(actor.id_hash_)) {
            spdlog::error("RpgCatalog: actors 文件 '{}' 存在重复 id '{}'", file_path, actor.id_);
            return false;
        }
        actors_.insert_or_assign(actor.id_hash_, std::move(actor));
    }

    return true;
}

bool RpgCatalog::loadSkills(const std::string_view file_path) {
    Json root{};
    if (!engine::utils::loadJsonObjectFile(file_path, root, "RpgCatalogSkills", spdlog::level::err)) {
        return false;
    }

    const auto skills_it = root.find("skills");
    if (skills_it == root.end() || !skills_it->is_array()) {
        spdlog::error("RpgCatalog: skills 文件 '{}' 缺少 skills 数组", file_path);
        return false;
    }

    skills_.clear();
    for (const auto& skill_node : *skills_it) {
        if (!skill_node.is_object()) {
            spdlog::error("RpgCatalog: skills 文件 '{}' 存在非 object 条目", file_path);
            return false;
        }

        SkillData skill{};
        skill.id_ = skill_node.value("id", std::string{});
        if (skill.id_.empty()) {
            spdlog::error("RpgCatalog: skills 文件 '{}' 存在空 id 条目", file_path);
            return false;
        }
        skill.id_hash_ = RpgCatalog::hashId(skill.id_);
        skill.display_name_ = skill_node.value("display_name", skill.id_);
        skill.description_ = skill_node.value("description", std::string{});

        const auto scope = scopeFromString(skill_node.value("scope", std::string{"none"}));
        if (!scope.has_value()) {
            spdlog::error("RpgCatalog: skill '{}' scope 非法", skill.id_);
            return false;
        }
        skill.scope_ = *scope;

        const auto hit_type = hitTypeFromString(skill_node.value("hit_type", std::string{"certain"}));
        if (!hit_type.has_value()) {
            spdlog::error("RpgCatalog: skill '{}' hit_type 非法", skill.id_);
            return false;
        }
        skill.hit_type_ = *hit_type;

        skill.success_rate_ = skill_node.value("success_rate", 100);
        if (!isValidRate(skill.success_rate_)) {
            spdlog::error("RpgCatalog: skill '{}' success_rate 必须在 [0, 100]", skill.id_);
            return false;
        }

        skill.repeats_ = skill_node.value("repeats", 1);
        if (skill.repeats_ <= 0) {
            spdlog::error("RpgCatalog: skill '{}' repeats 必须 > 0", skill.id_);
            return false;
        }

        const auto damage_it = skill_node.find("damage");
        if (damage_it == skill_node.end() || !parseDamageData(*damage_it, skill.damage_)) {
            spdlog::error("RpgCatalog: skill '{}' damage 配置非法", skill.id_);
            return false;
        }

        const auto effects_it = skill_node.find("effects");
        if (effects_it != skill_node.end() && !parseEffectList(*effects_it, skill.effects_)) {
            spdlog::error("RpgCatalog: skill '{}' effects 配置非法", skill.id_);
            return false;
        }

        if (skills_.contains(skill.id_hash_)) {
            spdlog::error("RpgCatalog: skills 文件 '{}' 存在重复 id '{}'", file_path, skill.id_);
            return false;
        }
        skills_.insert_or_assign(skill.id_hash_, std::move(skill));
    }

    return true;
}

bool RpgCatalog::loadStates(const std::string_view file_path) {
    Json root{};
    if (!engine::utils::loadJsonObjectFile(file_path, root, "RpgCatalogStates", spdlog::level::err)) {
        return false;
    }

    const auto states_it = root.find("states");
    if (states_it == root.end() || !states_it->is_array()) {
        spdlog::error("RpgCatalog: states 文件 '{}' 缺少 states 数组", file_path);
        return false;
    }

    states_.clear();
    for (const auto& state_node : *states_it) {
        if (!state_node.is_object()) {
            spdlog::error("RpgCatalog: states 文件 '{}' 存在非 object 条目", file_path);
            return false;
        }

        StateData state{};
        state.id_ = state_node.value("id", std::string{});
        if (state.id_.empty()) {
            spdlog::error("RpgCatalog: states 文件 '{}' 存在空 id 条目", file_path);
            return false;
        }
        state.id_hash_ = RpgCatalog::hashId(state.id_);
        state.display_name_ = state_node.value("display_name", state.id_);
        state.priority_ = state_node.value("priority", 50);
        state.min_turns_ = state_node.value("min_turns", 1);
        state.max_turns_ = state_node.value("max_turns", 1);
        if (state.min_turns_ <= 0 || state.max_turns_ < state.min_turns_) {
            spdlog::error("RpgCatalog: state '{}' 回合范围非法", state.id_);
            return false;
        }

        const auto traits_it = state_node.find("traits");
        if (traits_it != state_node.end() && !parseTraitList(*traits_it, state.traits_)) {
            spdlog::error("RpgCatalog: state '{}' traits 配置非法", state.id_);
            return false;
        }

        if (states_.contains(state.id_hash_)) {
            spdlog::error("RpgCatalog: states 文件 '{}' 存在重复 id '{}'", file_path, state.id_);
            return false;
        }
        states_.insert_or_assign(state.id_hash_, std::move(state));
    }

    return true;
}

bool RpgCatalog::loadEnemies(const std::string_view file_path) {
    Json root{};
    if (!engine::utils::loadJsonObjectFile(file_path, root, "RpgCatalogEnemies", spdlog::level::err)) {
        return false;
    }

    const auto enemies_it = root.find("enemies");
    if (enemies_it == root.end() || !enemies_it->is_array()) {
        spdlog::error("RpgCatalog: enemies 文件 '{}' 缺少 enemies 数组", file_path);
        return false;
    }

    enemies_.clear();
    for (const auto& enemy_node : *enemies_it) {
        if (!enemy_node.is_object()) {
            spdlog::error("RpgCatalog: enemies 文件 '{}' 存在非 object 条目", file_path);
            return false;
        }

        EnemyData enemy{};
        enemy.id_ = enemy_node.value("id", std::string{});
        if (enemy.id_.empty()) {
            spdlog::error("RpgCatalog: enemies 文件 '{}' 存在空 id 条目", file_path);
            return false;
        }
        enemy.id_hash_ = RpgCatalog::hashId(enemy.id_);
        enemy.display_name_ = enemy_node.value("display_name", enemy.id_);

        const auto params_it = enemy_node.find("params");
        if (params_it == enemy_node.end() || !parseParamArray(*params_it, enemy.params_)) {
            spdlog::error("RpgCatalog: enemy '{}' params 配置非法", enemy.id_);
            return false;
        }

        enemy.exp_reward_ = enemy_node.value("exp", 0);
        enemy.gold_reward_ = enemy_node.value("gold", 0);

        if (const auto drops_it = enemy_node.find("drops"); drops_it != enemy_node.end()) {
            if (!drops_it->is_array()) {
                spdlog::error("RpgCatalog: enemy '{}' drops 必须是数组", enemy.id_);
                return false;
            }
            for (const auto& drop_node : *drops_it) {
                if (!drop_node.is_object()) {
                    spdlog::error("RpgCatalog: enemy '{}' drops 存在非 object 条目", enemy.id_);
                    return false;
                }
                EnemyDropData drop{};
                drop.item_id_ = drop_node.value("item_id", std::string{});
                drop.chance_ = drop_node.value("chance", 0.0F);
                if (drop.item_id_.empty() || !isValidChance(drop.chance_)) {
                    spdlog::error("RpgCatalog: enemy '{}' drop 条目非法", enemy.id_);
                    return false;
                }
                enemy.drops_.push_back(std::move(drop));
            }
        }

        if (const auto actions_it = enemy_node.find("actions"); actions_it != enemy_node.end()) {
            if (!actions_it->is_array()) {
                spdlog::error("RpgCatalog: enemy '{}' actions 必须是数组", enemy.id_);
                return false;
            }
            for (const auto& action_node : *actions_it) {
                if (!action_node.is_object()) {
                    spdlog::error("RpgCatalog: enemy '{}' actions 存在非 object 条目", enemy.id_);
                    return false;
                }
                EnemyActionData action{};
                action.skill_id_ = action_node.value("skill_id", std::string{});
                action.rating_ = action_node.value("rating", 0);
                if (action.skill_id_.empty() || action.rating_ <= 0) {
                    spdlog::error("RpgCatalog: enemy '{}' action 条目非法", enemy.id_);
                    return false;
                }
                enemy.actions_.push_back(std::move(action));
            }
        }

        if (enemies_.contains(enemy.id_hash_)) {
            spdlog::error("RpgCatalog: enemies 文件 '{}' 存在重复 id '{}'", file_path, enemy.id_);
            return false;
        }
        enemies_.insert_or_assign(enemy.id_hash_, std::move(enemy));
    }

    return true;
}

bool RpgCatalog::loadTroops(const std::string_view file_path) {
    Json root{};
    if (!engine::utils::loadJsonObjectFile(file_path, root, "RpgCatalogTroops", spdlog::level::err)) {
        return false;
    }

    const auto troops_it = root.find("troops");
    if (troops_it == root.end() || !troops_it->is_array()) {
        spdlog::error("RpgCatalog: troops 文件 '{}' 缺少 troops 数组", file_path);
        return false;
    }

    troops_.clear();
    for (const auto& troop_node : *troops_it) {
        if (!troop_node.is_object()) {
            spdlog::error("RpgCatalog: troops 文件 '{}' 存在非 object 条目", file_path);
            return false;
        }

        TroopData troop{};
        troop.id_ = troop_node.value("id", std::string{});
        if (troop.id_.empty()) {
            spdlog::error("RpgCatalog: troops 文件 '{}' 存在空 id 条目", file_path);
            return false;
        }
        troop.id_hash_ = RpgCatalog::hashId(troop.id_);
        troop.display_name_ = troop_node.value("display_name", troop.id_);

        const auto members_it = troop_node.find("members");
        if (members_it == troop_node.end() || !members_it->is_array()) {
            spdlog::error("RpgCatalog: troop '{}' 缺少 members 数组", troop.id_);
            return false;
        }
        for (const auto& member_node : *members_it) {
            if (!member_node.is_object()) {
                spdlog::error("RpgCatalog: troop '{}' members 存在非 object 条目", troop.id_);
                return false;
            }
            TroopMemberData member{};
            member.enemy_id_ = member_node.value("enemy_id", std::string{});
            member.x_ = member_node.value("x", 0.0F);
            member.y_ = member_node.value("y", 0.0F);
            if (member.enemy_id_.empty()) {
                spdlog::error("RpgCatalog: troop '{}' member 缺少 enemy_id", troop.id_);
                return false;
            }
            troop.members_.push_back(std::move(member));
        }
        if (troop.members_.empty()) {
            spdlog::error("RpgCatalog: troop '{}' members 不能为空", troop.id_);
            return false;
        }

        if (troops_.contains(troop.id_hash_)) {
            spdlog::error("RpgCatalog: troops 文件 '{}' 存在重复 id '{}'", file_path, troop.id_);
            return false;
        }
        troops_.insert_or_assign(troop.id_hash_, std::move(troop));
    }

    return true;
}

bool RpgCatalog::validateReferences(std::string& out_error) const {
    for (const auto& [enemy_id, enemy] : enemies_) {
        (void)enemy_id;
        for (const auto& action : enemy.actions_) {
            const entt::id_type skill_id_hash = RpgCatalog::hashId(action.skill_id_);
            if (!skills_.contains(skill_id_hash)) {
                out_error = "Enemy '" + enemy.id_ + "' references missing skill '" + action.skill_id_ + "'";
                return false;
            }
        }
    }

    for (const auto& [troop_id, troop] : troops_) {
        (void)troop_id;
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

    // TODO: 当 ItemCatalog 接入统一引用校验后，补充 EnemyDropData::item_id_ 的跨 catalog 校验。
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
