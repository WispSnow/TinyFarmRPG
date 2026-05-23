#include "game/data/rpg_catalog_loaders.h"

#include "engine/utils/json_file_loader.h"
#include "game/data/rpg_catalog.h"
#include "game/data/rpg_catalog_parser_support.h"

#include <spdlog/spdlog.h>

#include <string>
#include <utility>

namespace game::data {

bool loadRpgEquipmentFile(const std::string_view file_path,
                          std::unordered_map<entt::id_type, EquipmentData>& out_equipment) {
    RpgCatalogJson root{};
    if (!engine::utils::loadJsonObjectFile(file_path, root, "RpgCatalogEquipment", spdlog::level::err)) {
        return false;
    }

    const auto equipment_it = root.find("equipment");
    if (equipment_it == root.end() || !equipment_it->is_array()) {
        spdlog::error("RpgCatalog: equipment 文件 '{}' 缺少 equipment 数组", file_path);
        return false;
    }

    out_equipment.clear();
    for (const auto& equipment_node : *equipment_it) {
        if (!equipment_node.is_object()) {
            spdlog::error("RpgCatalog: equipment 文件 '{}' 存在非 object 条目", file_path);
            return false;
        }

        EquipmentData equipment{};
        equipment.item_id_ = equipment_node.value("item_id", std::string{});
        if (equipment.item_id_.empty()) {
            spdlog::error("RpgCatalog: equipment 文件 '{}' 存在空 item_id 条目", file_path);
            return false;
        }
        equipment.item_id_hash_ = RpgCatalog::hashId(equipment.item_id_);

        const auto slot = equipmentSlotIdFromString(equipment_node.value("slot", std::string{}));
        if (!slot.has_value()) {
            spdlog::error("RpgCatalog: equipment '{}' slot 非法", equipment.item_id_);
            return false;
        }
        equipment.slot_ = *slot;

        if (const auto bonuses_it = equipment_node.find("param_bonuses");
            bonuses_it != equipment_node.end() && !parseRpgParamBonusArray(*bonuses_it, equipment.param_bonuses_)) {
            spdlog::error("RpgCatalog: equipment '{}' param_bonuses 配置非法", equipment.item_id_);
            return false;
        }

        if (const auto classes_it = equipment_node.find("allowed_classes");
            classes_it != equipment_node.end() && !parseRpgStringList(*classes_it, equipment.allowed_classes_)) {
            spdlog::error("RpgCatalog: equipment '{}' allowed_classes 必须是 string 数组", equipment.item_id_);
            return false;
        }

        if (const auto actors_it = equipment_node.find("allowed_actors");
            actors_it != equipment_node.end() && !parseRpgStringList(*actors_it, equipment.allowed_actors_)) {
            spdlog::error("RpgCatalog: equipment '{}' allowed_actors 必须是 string 数组", equipment.item_id_);
            return false;
        }

        if (out_equipment.contains(equipment.item_id_hash_)) {
            spdlog::error("RpgCatalog: equipment 文件 '{}' 存在重复 item_id '{}'", file_path, equipment.item_id_);
            return false;
        }
        out_equipment.insert_or_assign(equipment.item_id_hash_, std::move(equipment));
    }

    return true;
}

} // namespace game::data
