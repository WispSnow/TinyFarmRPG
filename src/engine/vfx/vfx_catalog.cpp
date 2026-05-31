#include "engine/vfx/vfx_catalog.h"

#include "engine/utils/json_file_loader.h"

#include <entt/core/hashed_string.hpp>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <utility>

namespace engine::vfx {

bool VfxCatalog::loadFromFile(std::string_view file_path) {
    nlohmann::json root{};
    if (!engine::utils::loadJsonObjectFile(file_path, root, "VfxCatalog", spdlog::level::err)) {
        return false;
    }

    const auto effects_it = root.find("effects");
    if (effects_it == root.end() || !effects_it->is_object()) {
        spdlog::error("VfxCatalog: '{}' 缺少 effects 对象", file_path);
        return false;
    }

    std::unordered_map<entt::id_type, std::string> next_effect_paths{};
    for (const auto& [effect_key, value] : effects_it->items()) {
        if (!value.is_string()) {
            continue;
        }
        const auto effect_path = value.get<std::string>();
        if (effect_path.empty()) {
            continue;
        }

        const entt::id_type effect_id = entt::hashed_string{effect_key.c_str(), effect_key.size()}.value();
        next_effect_paths[effect_id] = effect_path;
    }

    if (next_effect_paths.empty()) {
        spdlog::warn("VfxCatalog: '{}' 未解析到任何有效特效路径，将以空目录继续运行", file_path);
    }

    effect_paths_ = std::move(next_effect_paths);
    return true;
}

const std::string* VfxCatalog::findEffectPath(const entt::id_type effect_id) const {
    if (const auto it = effect_paths_.find(effect_id); it != effect_paths_.end()) {
        return &it->second;
    }
    return nullptr;
}

} // namespace engine::vfx
