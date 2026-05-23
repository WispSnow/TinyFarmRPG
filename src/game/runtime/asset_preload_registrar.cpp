#include "asset_preload_registrar.h"

#include "engine/render/image.h"
#include "engine/resource/asset_registry.h"
#include "engine/utils/json_file_loader.h"
#include "game/data/appearance_catalog.h"
#include "game/data/item_catalog.h"
#include "game/factory/blueprint_manager.h"
#include "game/world/world_state.h"

#include <entt/core/hashed_string.hpp>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_set>

namespace {

[[nodiscard]] entt::id_type hashPath(std::string_view path) {
    if (path.empty()) {
        return entt::null;
    }
    return entt::hashed_string{path.data(), path.size()}.value();
}

void registerTexturePath(engine::resource::AssetRegistry& registry, entt::id_type id, std::string_view path) {
    if (path.empty()) {
        return;
    }
    const entt::id_type resolved_id = (id != entt::null) ? id : hashPath(path);
    if (resolved_id == entt::null) {
        return;
    }
    registry.registerTexture(resolved_id, path);
}

void registerImageTexture(engine::resource::AssetRegistry& registry, const engine::render::Image& image) {
    registerTexturePath(registry, image.getTextureId(), image.getTexturePath());
}

[[nodiscard]] std::string resolveRelativePath(std::string_view relative_path, std::string_view anchor_file) {
    const auto anchor_dir = std::filesystem::path(anchor_file).parent_path();
    return (anchor_dir / std::filesystem::path(relative_path)).lexically_normal().string();
}

void registerTilesetTexturesFromTsj(std::string_view tsj_path, engine::resource::AssetRegistry& registry) {
    nlohmann::json tsj_json;
    if (!engine::utils::loadJsonObjectFile(tsj_path, tsj_json, "AssetRegistry")) {
        return;
    }

    if (const auto image_it = tsj_json.find("image");
        image_it != tsj_json.end() && image_it->is_string() && !image_it->get_ref<const std::string&>().empty()) {
        const auto texture_path = resolveRelativePath(image_it->get_ref<const std::string&>(), tsj_path);
        registerTexturePath(registry, hashPath(texture_path), texture_path);
    }

    if (const auto tiles_it = tsj_json.find("tiles"); tiles_it != tsj_json.end() && tiles_it->is_array()) {
        for (const auto& tile_json : *tiles_it) {
            if (!tile_json.is_object()) {
                continue;
            }
            if (const auto tile_image_it = tile_json.find("image");
                tile_image_it != tile_json.end() && tile_image_it->is_string() &&
                !tile_image_it->get_ref<const std::string&>().empty()) {
                const auto texture_path = resolveRelativePath(tile_image_it->get_ref<const std::string&>(), tsj_path);
                registerTexturePath(registry, hashPath(texture_path), texture_path);
            }
        }
    }
}

void registerMapTilesetTextures(std::string_view tmj_path,
                                engine::resource::AssetRegistry& registry,
                                std::unordered_set<std::string>& scanned_tilesets) {
    nlohmann::json map_json;
    if (!engine::utils::loadJsonObjectFile(tmj_path, map_json, "AssetRegistry")) {
        return;
    }

    if (const auto layers_it = map_json.find("layers"); layers_it != map_json.end() && layers_it->is_array()) {
        for (const auto& layer_json : *layers_it) {
            if (!layer_json.is_object()) {
                continue;
            }
            const std::string layer_type = layer_json.value("type", "");
            if (layer_type != "imagelayer") {
                continue;
            }
            const std::string image_path = layer_json.value("image", "");
            if (image_path.empty()) {
                continue;
            }
            const auto resolved_path = resolveRelativePath(image_path, tmj_path);
            registerTexturePath(registry, hashPath(resolved_path), resolved_path);
        }
    }

    const auto tilesets_it = map_json.find("tilesets");
    if (tilesets_it == map_json.end() || !tilesets_it->is_array()) {
        return;
    }

    for (const auto& tileset_ref : *tilesets_it) {
        if (!tileset_ref.is_object()) {
            continue;
        }

        if (const auto source_it = tileset_ref.find("source");
            source_it != tileset_ref.end() && source_it->is_string()) {
            const auto tsj_path = resolveRelativePath(source_it->get_ref<const std::string&>(), tmj_path);
            if (!scanned_tilesets.insert(tsj_path).second) {
                continue;
            }
            registerTilesetTexturesFromTsj(tsj_path, registry);
            continue;
        }

        if (const auto image_it = tileset_ref.find("image"); image_it != tileset_ref.end() && image_it->is_string()) {
            const auto texture_path = resolveRelativePath(image_it->get_ref<const std::string&>(), tmj_path);
            registerTexturePath(registry, hashPath(texture_path), texture_path);
        }
    }
}

} // namespace

namespace game::runtime {

void AssetPreloadRegistrar::collectBlueprintAssets(const game::factory::BlueprintManager& manager,
                                                   engine::resource::AssetRegistry& registry) {
    const auto collect_animations = [&registry](const auto& animations) {
        for (const auto& [_, animation] : animations) {
            registerTexturePath(registry, animation.texture_id_, animation.texture_path_);
        }
    };

    for (const auto& [_, blueprint] : manager.actorBlueprints()) {
        registerTexturePath(registry, blueprint.sprite_.id_, blueprint.sprite_.path_);
        collect_animations(blueprint.animations_);
    }

    for (const auto& [_, blueprint] : manager.animalBlueprints()) {
        registerTexturePath(registry, blueprint.sprite_.id_, blueprint.sprite_.path_);
        collect_animations(blueprint.animations_);
    }

    for (const auto& [_, blueprint] : manager.cropBlueprints()) {
        for (const auto& stage : blueprint.stages_) {
            registerTexturePath(registry, stage.sprite_.id_, stage.sprite_.path_);
        }
    }
}

void AssetPreloadRegistrar::collectItemCatalogAssets(const game::data::ItemCatalog& catalog,
                                                     engine::resource::AssetRegistry& registry) {
    for (const auto& [_, icon] : catalog.icons()) {
        registerImageTexture(registry, icon);
    }
}

void AssetPreloadRegistrar::collectAppearanceAssets(const game::data::AppearanceCatalog& catalog,
                                                    engine::resource::AssetRegistry& registry) {
    constexpr std::size_t kRuntimeVariantPreloadLimitPerSlot = 3;

    const auto* profile = catalog.defaultProfile();
    if (!profile) {
        return;
    }

    const auto preload_paths = catalog.collectPreloadTexturePaths(*profile, kRuntimeVariantPreloadLimitPerSlot);
    for (const auto& path : preload_paths) {
        registerTexturePath(registry, hashPath(path), path);
    }
}

void AssetPreloadRegistrar::collectWorldMapAssets(const game::world::WorldState& world_state,
                                                  engine::resource::AssetRegistry& registry) {
    std::unordered_set<std::string> scanned_tilesets{};
    for (const auto& [_, map_state] : world_state.maps()) {
        if (map_state.info.file_path.empty()) {
            continue;
        }
        registerMapTilesetTextures(map_state.info.file_path, registry, scanned_tilesets);
    }
}

} // namespace game::runtime
