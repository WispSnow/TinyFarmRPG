#include "game/data/rpg_catalog_loaders.h"

#include "engine/utils/json_file_loader.h"
#include "game/data/rpg_catalog.h"
#include "game/data/rpg_catalog_parser_support.h"

#include <spdlog/spdlog.h>

#include <string>
#include <utility>

namespace game::data {

bool loadRpgTroopsFile(const std::string_view file_path,
                       std::unordered_map<entt::id_type, TroopData>& out_troops) {
    RpgCatalogJson root{};
    if (!engine::utils::loadJsonObjectFile(file_path, root, "RpgCatalogTroops", spdlog::level::err)) {
        return false;
    }

    const auto troops_it = root.find("troops");
    if (troops_it == root.end() || !troops_it->is_array()) {
        spdlog::error("RpgCatalog: troops 文件 '{}' 缺少 troops 数组", file_path);
        return false;
    }

    std::unordered_map<entt::id_type, TroopData> parsed_troops{};
    parsed_troops.reserve(troops_it->size());
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
        troop.battle_background_id_ = troop_node.value("battle_background_id", std::string{});

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

        if (parsed_troops.contains(troop.id_hash_)) {
            spdlog::error("RpgCatalog: troops 文件 '{}' 存在重复 id '{}'", file_path, troop.id_);
            return false;
        }
        parsed_troops.insert_or_assign(troop.id_hash_, std::move(troop));
    }

    out_troops = std::move(parsed_troops);
    return true;
}

} // namespace game::data
