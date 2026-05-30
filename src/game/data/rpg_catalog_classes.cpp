#include "game/data/rpg_catalog_loaders.h"

#include "engine/utils/json_file_loader.h"
#include "game/data/rpg_catalog.h"
#include "game/data/rpg_catalog_parser_support.h"

#include <spdlog/spdlog.h>

#include <string>
#include <utility>

namespace game::data {

bool loadRpgClassesFile(const std::string_view file_path,
                        std::unordered_map<entt::id_type, ClassData>& out_classes) {
    RpgCatalogJson root{};
    if (!engine::utils::loadJsonObjectFile(file_path, root, "RpgCatalogClasses", spdlog::level::err)) {
        return false;
    }

    const auto classes_it = root.find("classes");
    if (classes_it == root.end() || !classes_it->is_array()) {
        spdlog::error("RpgCatalog: classes 文件 '{}' 缺少 classes 数组", file_path);
        return false;
    }

    std::unordered_map<entt::id_type, ClassData> parsed_classes{};
    parsed_classes.reserve(classes_it->size());
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
        if (params_it == class_node.end() || !parseRpgParamArray(*params_it, klass.base_params_)) {
            spdlog::error("RpgCatalog: class '{}' base_params 配置非法", klass.id_);
            return false;
        }

        if (const auto exp_curve_it = class_node.find("exp_curve");
            exp_curve_it != class_node.end() && !parseRpgExpCurve(*exp_curve_it, klass.exp_curve_)) {
            spdlog::error("RpgCatalog: class '{}' exp_curve 配置非法", klass.id_);
            return false;
        }

        if (const auto param_curves_it = class_node.find("param_curves");
            param_curves_it != class_node.end() &&
            !parseRpgParamCurves(*param_curves_it, klass.base_params_, klass.param_curves_, klass.has_param_curves_)) {
            spdlog::error("RpgCatalog: class '{}' param_curves 配置非法", klass.id_);
            return false;
        }

        if (parsed_classes.contains(klass.id_hash_)) {
            spdlog::error("RpgCatalog: classes 文件 '{}' 存在重复 id '{}'", file_path, klass.id_);
            return false;
        }
        parsed_classes.insert_or_assign(klass.id_hash_, std::move(klass));
    }

    out_classes = std::move(parsed_classes);
    return true;
}

} // namespace game::data
