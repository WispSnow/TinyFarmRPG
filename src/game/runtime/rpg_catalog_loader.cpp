#include "game/runtime/rpg_catalog_loader.h"

#include "game/data/rpg_catalog.h"

#include <filesystem>
#include <optional>
#include <string>

namespace {

[[nodiscard]] std::string makeLoadError(std::string_view label, const std::string& path) {
    std::string message{"加载 RPG "};
    message += label;
    message += " 失败: ";
    message += path;
    return message;
}

[[nodiscard]] std::optional<std::string> resolveRpgFilePath(const game::data::RpgCatalog& catalog,
                                                            const std::filesystem::path& root_path,
                                                            std::string_view key) {
    const auto* manifest = catalog.manifest();
    if (!manifest) {
        return std::nullopt;
    }

    const auto it = manifest->files_.find(std::string{key});
    if (it == manifest->files_.end() || it->second.empty()) {
        return std::nullopt;
    }

    return (root_path / it->second).generic_string();
}

} // namespace

namespace game::runtime {

bool loadRpgCatalogFromManifest(game::data::RpgCatalog& catalog,
                                const RpgCatalogLoadOptions& options,
                                std::string& out_error) {
    out_error.clear();

    if (options.manifest_path.empty()) {
        out_error = "RPG manifest 路径为空。";
        return false;
    }
    if (options.root_path.empty()) {
        out_error = "RPG root 路径为空。";
        return false;
    }

    if (!catalog.loadManifest(options.manifest_path)) {
        out_error = "加载 RPG manifest 失败: " + options.manifest_path;
        return false;
    }

    if (!catalog.manifest()) {
        out_error = "RPG manifest 未加载。";
        return false;
    }

    const std::filesystem::path root_path{options.root_path};
    const auto classes_path = resolveRpgFilePath(catalog, root_path, "classes");
    const auto actors_path = resolveRpgFilePath(catalog, root_path, "actors");
    const auto skills_path = resolveRpgFilePath(catalog, root_path, "skills");
    const auto states_path = resolveRpgFilePath(catalog, root_path, "states");
    const auto equipment_path = resolveRpgFilePath(catalog, root_path, "equipment");
    const auto enemies_path = resolveRpgFilePath(catalog, root_path, "enemies");
    const auto troops_path = resolveRpgFilePath(catalog, root_path, "troops");

    if (!classes_path || !actors_path || !skills_path || !states_path || !equipment_path || !enemies_path || !troops_path) {
        out_error = "RPG manifest 缺少 classes/actors/skills/states/equipment/enemies/troops 文件映射。";
        return false;
    }

    if (!catalog.loadClasses(*classes_path)) {
        out_error = makeLoadError("classes", *classes_path);
        return false;
    }
    if (!catalog.loadActors(*actors_path)) {
        out_error = makeLoadError("actors", *actors_path);
        return false;
    }
    if (!catalog.loadSkills(*skills_path)) {
        out_error = makeLoadError("skills", *skills_path);
        return false;
    }
    if (!catalog.loadStates(*states_path)) {
        out_error = makeLoadError("states", *states_path);
        return false;
    }
    if (!catalog.loadEquipment(*equipment_path)) {
        out_error = makeLoadError("equipment", *equipment_path);
        return false;
    }
    if (!catalog.loadEnemies(*enemies_path)) {
        out_error = makeLoadError("enemies", *enemies_path);
        return false;
    }
    if (!catalog.loadTroops(*troops_path)) {
        out_error = makeLoadError("troops", *troops_path);
        return false;
    }

    std::string reference_error{};
    if (!catalog.validateReferences(reference_error, options.item_catalog)) {
        out_error = "RPG 引用校验失败: " + reference_error;
        return false;
    }

    return true;
}

} // namespace game::runtime
