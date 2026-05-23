#include "game/data/rpg_catalog_loaders.h"

#include "engine/utils/json_file_loader.h"
#include "game/data/rpg_catalog.h"
#include "game/data/rpg_catalog_parser_support.h"

#include <spdlog/spdlog.h>

#include <string>
#include <utility>

namespace game::data {

bool loadRpgSkillsFile(const std::string_view file_path,
                       std::unordered_map<entt::id_type, SkillData>& out_skills) {
    RpgCatalogJson root{};
    if (!engine::utils::loadJsonObjectFile(file_path, root, "RpgCatalogSkills", spdlog::level::err)) {
        return false;
    }

    const auto skills_it = root.find("skills");
    if (skills_it == root.end() || !skills_it->is_array()) {
        spdlog::error("RpgCatalog: skills 文件 '{}' 缺少 skills 数组", file_path);
        return false;
    }

    out_skills.clear();
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

        skill.mp_cost_ = skill_node.value("mp_cost", 0);
        if (skill.mp_cost_ < 0) {
            spdlog::error("RpgCatalog: skill '{}' mp_cost 必须 >= 0", skill.id_);
            return false;
        }

        skill.success_rate_ = skill_node.value("success_rate", 100);
        if (!isValidRpgRate(skill.success_rate_)) {
            spdlog::error("RpgCatalog: skill '{}' success_rate 必须在 [0, 100]", skill.id_);
            return false;
        }

        skill.repeats_ = skill_node.value("repeats", 1);
        if (skill.repeats_ <= 0) {
            spdlog::error("RpgCatalog: skill '{}' repeats 必须 > 0", skill.id_);
            return false;
        }

        const auto damage_it = skill_node.find("damage");
        if (damage_it == skill_node.end() || !parseRpgDamageData(*damage_it, skill.damage_)) {
            spdlog::error("RpgCatalog: skill '{}' damage 配置非法", skill.id_);
            return false;
        }

        const auto effects_it = skill_node.find("effects");
        if (effects_it != skill_node.end() && !parseRpgEffectList(*effects_it, skill.effects_)) {
            spdlog::error("RpgCatalog: skill '{}' effects 配置非法", skill.id_);
            return false;
        }

        if (const auto presentation_it = skill_node.find("presentation");
            presentation_it != skill_node.end() &&
            !parseRpgSkillPresentation(*presentation_it, skill.presentation_)) {
            spdlog::error(
                "RpgCatalog: skill '{}' presentation 非法，需满足 duration > 0, 0 <= impact_time <= duration, "
                "duration 覆盖 recovery_duration / target_vfx_tail，且 target_vfx_scale > 0",
                skill.id_);
            return false;
        }

        if (out_skills.contains(skill.id_hash_)) {
            spdlog::error("RpgCatalog: skills 文件 '{}' 存在重复 id '{}'", file_path, skill.id_);
            return false;
        }
        out_skills.insert_or_assign(skill.id_hash_, std::move(skill));
    }

    return true;
}

} // namespace game::data
