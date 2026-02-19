#pragma once

#include <entt/core/fwd.hpp>

#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>

namespace engine::resource {

class AssetRegistry final {
public:
    struct FontKey {
        entt::id_type id{};
        int pixel_size{0};

        [[nodiscard]] bool operator==(const FontKey&) const noexcept = default;
    };

    struct FontKeyHash {
        [[nodiscard]] std::size_t operator()(const FontKey& key) const noexcept {
            const std::size_t h1 = std::hash<entt::id_type>{}(key.id);
            const std::size_t h2 = std::hash<int>{}(key.pixel_size);
            return h1 ^ (h2 + 0x9e3779b9u + (h1 << 6u) + (h1 >> 2u));
        }
    };

    void registerTexture(entt::id_type id, std::string_view path);
    void registerFont(entt::id_type id, int pixel_size, std::string_view path);
    void registerSound(entt::id_type id, std::string_view path);
    void registerMusic(entt::id_type id, std::string_view path);

    [[nodiscard]] std::string_view findTexturePath(entt::id_type id) const;
    [[nodiscard]] std::string_view findFontPath(entt::id_type id, int pixel_size) const;
    [[nodiscard]] std::string_view findSoundPath(entt::id_type id) const;
    [[nodiscard]] std::string_view findMusicPath(entt::id_type id) const;

private:
    std::unordered_map<entt::id_type, std::string> texture_paths_{};
    std::unordered_map<FontKey, std::string, FontKeyHash> font_paths_{};
    std::unordered_map<entt::id_type, std::string> sound_paths_{};
    std::unordered_map<entt::id_type, std::string> music_paths_{};
};

} // namespace engine::resource
