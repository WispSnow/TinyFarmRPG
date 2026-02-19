#include "asset_registry.h"

#include <entt/entity/entity.hpp>
#include <spdlog/spdlog.h>

#include <cstdint>

namespace engine::resource {

namespace {

template <typename Map, typename Key, typename KeyToString>
void registerPath(Map& paths,
                  const Key& key,
                  std::string_view path,
                  std::string_view kind,
                  KeyToString&& key_to_string) {
    if (path.empty()) {
        return;
    }

#ifndef NDEBUG
    if (const auto it = paths.find(key); it != paths.end() && it->second != path) {
        spdlog::warn(
            "AssetRegistry: {} ID={} 映射冲突: '{}' -> '{}'",
            kind,
            key_to_string(key),
            it->second,
            path
        );
    }
#endif

    paths.insert_or_assign(key, std::string(path));
}

} // namespace

void AssetRegistry::registerTexture(entt::id_type id, std::string_view path) {
    if (id == entt::null || path.empty()) {
        return;
    }
    registerPath(
        texture_paths_,
        id,
        path,
        "texture",
        [](entt::id_type value) { return static_cast<std::uint64_t>(value); }
    );
}

void AssetRegistry::registerFont(entt::id_type id, int pixel_size, std::string_view path) {
    if (id == entt::null || pixel_size <= 0 || path.empty()) {
        return;
    }
    const FontKey key{id, pixel_size};
    registerPath(
        font_paths_,
        key,
        path,
        "font",
        [](const FontKey& value) {
            return std::to_string(static_cast<std::uint64_t>(value.id)) + "@" + std::to_string(value.pixel_size);
        }
    );
}

void AssetRegistry::registerSound(entt::id_type id, std::string_view path) {
    if (id == entt::null || path.empty()) {
        return;
    }
    registerPath(
        sound_paths_,
        id,
        path,
        "sound",
        [](entt::id_type value) { return static_cast<std::uint64_t>(value); }
    );
}

void AssetRegistry::registerMusic(entt::id_type id, std::string_view path) {
    if (id == entt::null || path.empty()) {
        return;
    }
    registerPath(
        music_paths_,
        id,
        path,
        "music",
        [](entt::id_type value) { return static_cast<std::uint64_t>(value); }
    );
}

std::string_view AssetRegistry::findTexturePath(entt::id_type id) const {
    if (const auto it = texture_paths_.find(id); it != texture_paths_.end()) {
        return it->second;
    }
    return {};
}

std::string_view AssetRegistry::findFontPath(entt::id_type id, int pixel_size) const {
    const FontKey key{id, pixel_size};
    if (const auto it = font_paths_.find(key); it != font_paths_.end()) {
        return it->second;
    }
    return {};
}

std::string_view AssetRegistry::findSoundPath(entt::id_type id) const {
    if (const auto it = sound_paths_.find(id); it != sound_paths_.end()) {
        return it->second;
    }
    return {};
}

std::string_view AssetRegistry::findMusicPath(entt::id_type id) const {
    if (const auto it = music_paths_.find(id); it != music_paths_.end()) {
        return it->second;
    }
    return {};
}

} // namespace engine::resource
