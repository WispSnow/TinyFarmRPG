#include "game/data/rpg_catalog_loaders.h"

#include "engine/utils/json_file_loader.h"
#include "game/data/rpg_catalog.h"
#include "game/data/rpg_catalog_parser_support.h"

#include <spdlog/spdlog.h>

#include <string>
#include <utility>

namespace game::data {

bool loadRpgEnemiesFile(const std::string_view file_path,
                        std::unordered_map<entt::id_type, EnemyData>& out_enemies) {
    RpgCatalogJson root{};
    if (!engine::utils::loadJsonObjectFile(file_path, root, "RpgCatalogEnemies", spdlog::level::err)) {
        return false;
    }

    const auto enemies_it = root.find("enemies");
    if (enemies_it == root.end() || !enemies_it->is_array()) {
        spdlog::error("RpgCatalog: enemies 文件 '{}' 缺少 enemies 数组", file_path);
        return false;
    }

    out_enemies.clear();
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
        if (params_it == enemy_node.end() || !parseRpgParamArray(*params_it, enemy.params_)) {
            spdlog::error("RpgCatalog: enemy '{}' params 配置非法", enemy.id_);
            return false;
        }

        enemy.exp_reward_ = enemy_node.value("exp", 0);
        enemy.gold_reward_ = enemy_node.value("gold", 0);

        if (const auto visual_it = enemy_node.find("battle_visual");
            visual_it != enemy_node.end() && !parseRpgBattleVisual(*visual_it, enemy.battle_visual_)) {
            spdlog::error("RpgCatalog: enemy '{}' battle_visual 配置非法", enemy.id_);
            return false;
        }

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
                if (drop.item_id_.empty() || !isValidRpgChance(drop.chance_)) {
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

        if (out_enemies.contains(enemy.id_hash_)) {
            spdlog::error("RpgCatalog: enemies 文件 '{}' 存在重复 id '{}'", file_path, enemy.id_);
            return false;
        }
        out_enemies.insert_or_assign(enemy.id_hash_, std::move(enemy));
    }

    return true;
}

} // namespace game::data
