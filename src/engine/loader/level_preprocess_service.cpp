#include "engine/loader/level_preprocess_service.h"

#include "engine/loader/tiled_json_cache.h"
#include "engine/loader/tiled_json_helpers.h"

#include <algorithm>
#include <filesystem>
#include <map>
#include <unordered_set>

namespace engine::loader {

namespace {

[[nodiscard]] bool isValidTilesetRef(const nlohmann::json& tileset_ref) {
    if (!tileset_ref.is_object()) {
        return false;
    }
    const auto source = tiled::findStringMember(tileset_ref, "source");
    if (!source || source->empty()) {
        return false;
    }
    return tiled::jsonIntOr(tileset_ref, "firstgid", 0) > 0;
}

[[nodiscard]] std::string resolvePath(std::string_view relative_path, std::string_view file_path) {
    const auto map_dir = std::filesystem::path(file_path).parent_path();
    const auto combined_path = map_dir / relative_path;
    return combined_path.lexically_normal().string();
}

template <typename Container>
void appendUniquePaths(std::vector<std::string>& out, const Container& paths) {
    out.reserve(out.size() + paths.size());
    for (const auto& p : paths) {
        out.push_back(p);
    }
    std::ranges::sort(out);
    out.erase(std::unique(out.begin(), out.end()), out.end());
}

} // namespace

LevelPreprocessResult LevelPreprocessService::preprocessLevel(std::string_view level_path) {
    LevelPreprocessResult result{};
    result.data.level_path = std::string(level_path);
    if (level_path.empty()) {
        result.error = "level_path is empty";
        return result;
    }

    const auto json_ptr = tiled::getOrLoadLevelJson(level_path);
    if (!json_ptr) {
        result.error = "failed to load level json";
        return result;
    }

    const auto& level_json = *json_ptr;
    std::unordered_set<std::string> texture_paths{};
    std::map<std::string, int> tileset_entries{};

    if (const auto* tilesets = tiled::findMember(level_json, "tilesets"); tilesets && tilesets->is_array()) {
        for (const auto& tileset_ref : *tilesets) {
            if (!isValidTilesetRef(tileset_ref)) {
                continue;
            }

            const auto* source = tiled::findStringMember(tileset_ref, "source");
            const std::string tileset_path = resolvePath(*source, level_path);
            const int first_gid = tiled::jsonIntOr(tileset_ref, "firstgid", 0);
            if (first_gid > 0) {
                if (auto [it, inserted] = tileset_entries.emplace(tileset_path, first_gid); !inserted) {
                    it->second = std::min(it->second, first_gid);
                }
            }

            const auto tileset_json_ptr = tiled::getOrLoadTilesetJson(tileset_path);
            if (!tileset_json_ptr) {
                continue;
            }

            if (const auto* image = tiled::findStringMember(*tileset_json_ptr, "image"); image && !image->empty()) {
                texture_paths.insert(resolvePath(*image, tileset_path));
            }
        }
    }

    if (const auto* layers = tiled::findMember(level_json, "layers"); layers && layers->is_array()) {
        for (const auto& layer_json : *layers) {
            if (!tiled::jsonBoolOr(layer_json, "visible", true)) {
                continue;
            }
            if (tiled::jsonStringOr(layer_json, "type", "") != "imagelayer") {
                continue;
            }
            const auto image_path = tiled::jsonStringOr(layer_json, "image", "");
            if (image_path.empty()) {
                continue;
            }
            texture_paths.insert(resolvePath(image_path, level_path));
        }
    }

    appendUniquePaths(result.data.texture_paths, texture_paths);
    result.data.tilesets.reserve(tileset_entries.size());
    result.data.tileset_paths.reserve(tileset_entries.size());
    for (const auto& [path, first_gid] : tileset_entries) {
        result.data.tilesets.push_back(LevelPreprocessTileset{
            .first_gid = first_gid,
            .path = path,
        });
        result.data.tileset_paths.push_back(path);
    }
    result.success = true;
    return result;
}

} // namespace engine::loader
