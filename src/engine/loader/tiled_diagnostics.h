#pragma once

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

namespace engine::loader::tiled::diagnostics {

struct UnknownTileFlagToken {
    std::string tileset_path{};
    int tile_local_id{0};
    std::string token{};
    int count{0};
};

struct UnknownObjectType {
    std::string type{};
    std::string name{};
    bool point{false};
    int count{0};
};

struct UnknownLightName {
    std::string name{};
    bool point{false};
    int count{0};
};

inline std::vector<UnknownTileFlagToken> g_unknown_tile_flag_tokens{};
inline std::vector<UnknownObjectType> g_unknown_object_types{};
inline std::vector<UnknownLightName> g_unknown_light_names{};

inline void clearAll() {
    g_unknown_tile_flag_tokens.clear();
    g_unknown_object_types.clear();
    g_unknown_light_names.clear();
}

[[nodiscard]] inline const std::vector<UnknownTileFlagToken>& unknownTileFlagTokens() {
    return g_unknown_tile_flag_tokens;
}

[[nodiscard]] inline const std::vector<UnknownObjectType>& unknownObjectTypes() {
    return g_unknown_object_types;
}

[[nodiscard]] inline const std::vector<UnknownLightName>& unknownLightNames() {
    return g_unknown_light_names;
}

inline void recordUnknownTileFlagToken(std::string_view tileset_path, int tile_local_id, std::string_view token) {
    if (tileset_path.empty() || token.empty()) {
        return;
    }

    const std::string tileset_key(tileset_path);
    const std::string token_key(token);
    auto it = std::find_if(g_unknown_tile_flag_tokens.begin(),
                           g_unknown_tile_flag_tokens.end(),
                           [&](const UnknownTileFlagToken& entry) {
                               return entry.tile_local_id == tile_local_id &&
                                      entry.tileset_path == tileset_key &&
                                      entry.token == token_key;
                           });
    if (it != g_unknown_tile_flag_tokens.end()) {
        it->count += 1;
        return;
    }

    UnknownTileFlagToken entry{};
    entry.tileset_path = tileset_key;
    entry.tile_local_id = tile_local_id;
    entry.token = token_key;
    entry.count = 1;
    g_unknown_tile_flag_tokens.push_back(std::move(entry));
}

inline void recordUnknownObjectType(std::string_view type, std::string_view name, bool point) {
    if (type.empty()) {
        return;
    }

    const std::string type_key(type);
    const std::string name_key(name);
    auto it = std::find_if(g_unknown_object_types.begin(),
                           g_unknown_object_types.end(),
                           [&](const UnknownObjectType& entry) {
                               return entry.point == point && entry.type == type_key && entry.name == name_key;
                           });
    if (it != g_unknown_object_types.end()) {
        it->count += 1;
        return;
    }

    UnknownObjectType entry{};
    entry.type = type_key;
    entry.name = name_key;
    entry.point = point;
    entry.count = 1;
    g_unknown_object_types.push_back(std::move(entry));
}

inline void recordUnknownLightName(std::string_view name, bool point) {
    if (name.empty()) {
        return;
    }

    const std::string name_key(name);
    auto it = std::find_if(g_unknown_light_names.begin(),
                           g_unknown_light_names.end(),
                           [&](const UnknownLightName& entry) { return entry.point == point && entry.name == name_key; });
    if (it != g_unknown_light_names.end()) {
        it->count += 1;
        return;
    }

    UnknownLightName entry{};
    entry.name = name_key;
    entry.point = point;
    entry.count = 1;
    g_unknown_light_names.push_back(std::move(entry));
}

} // namespace engine::loader::tiled::diagnostics

