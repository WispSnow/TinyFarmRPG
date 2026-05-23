#include "game/data/rpg_catalog_loaders.h"

#include "engine/utils/json_file_loader.h"
#include "game/data/rpg_catalog.h"
#include "game/data/rpg_catalog_parser_support.h"

#include <entt/entity/entity.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <string>
#include <utility>

namespace game::data {

bool loadRpgActorsFile(const std::string_view file_path,
                       std::unordered_map<entt::id_type, ActorData>& out_actors) {
    RpgCatalogJson root{};
    if (!engine::utils::loadJsonObjectFile(file_path, root, "RpgCatalogActors", spdlog::level::err)) {
        return false;
    }

    const auto actors_it = root.find("actors");
    if (actors_it == root.end() || !actors_it->is_array()) {
        spdlog::error("RpgCatalog: actors 文件 '{}' 缺少 actors 数组", file_path);
        return false;
    }

    out_actors.clear();
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
        actor.map_actor_id_ = actor_node.value("map_actor_id", std::string{});
        actor.map_actor_id_hash_ = actor.map_actor_id_.empty() ? entt::null : RpgCatalog::hashId(actor.map_actor_id_);

        if (const auto visual_it = actor_node.find("battle_visual");
            visual_it != actor_node.end() && !parseRpgBattleVisual(*visual_it, actor.battle_visual_)) {
            spdlog::error("RpgCatalog: actor '{}' battle_visual 配置非法", actor.id_);
            return false;
        }

        if (actor.class_id_.empty()) {
            spdlog::error("RpgCatalog: actor '{}' 缺少 class_id", actor.id_);
            return false;
        }

        if (const auto skills_it = actor_node.find("skill_ids");
            skills_it != actor_node.end() && !parseRpgStringList(*skills_it, actor.skill_ids_)) {
            spdlog::error("RpgCatalog: actor '{}' skill_ids 必须是 string 数组", actor.id_);
            return false;
        }

        if (const auto portrait_it = actor_node.find("portrait");
            portrait_it != actor_node.end() && !parseRpgPortraitRef(*portrait_it, actor.portrait_)) {
            spdlog::error("RpgCatalog: actor '{}' portrait 配置非法", actor.id_);
            return false;
        }

        if (out_actors.contains(actor.id_hash_)) {
            spdlog::error("RpgCatalog: actors 文件 '{}' 存在重复 id '{}'", file_path, actor.id_);
            return false;
        }
        out_actors.insert_or_assign(actor.id_hash_, std::move(actor));
    }

    return true;
}

} // namespace game::data
