#pragma once

#include <deque>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace engine::loader::tiled {

using JsonPtr = std::shared_ptr<nlohmann::json>;

inline constexpr std::size_t LEVEL_JSON_CACHE_MAX_ENTRIES = 32;
inline constexpr std::size_t TILESET_JSON_CACHE_MAX_ENTRIES = 64;

inline std::unordered_map<std::string, JsonPtr> g_level_json_cache{};
inline std::deque<std::string> g_level_json_cache_order{};

inline std::unordered_map<std::string, JsonPtr> g_tileset_json_cache{};
inline std::deque<std::string> g_tileset_json_cache_order{};

[[nodiscard]] inline JsonPtr loadJsonFromFile(std::string_view file_path, std::string_view kind) {
    std::ifstream file{std::filesystem::path(std::string(file_path))};
    if (!file.is_open()) {
        spdlog::error("无法打开 {} JSON 文件: {}", kind, file_path);
        return nullptr;
    }

    const std::string file_content(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>()
    );

    nlohmann::json parsed = nlohmann::json::parse(file_content, nullptr, false);
    if (parsed.is_discarded()) {
        spdlog::error("解析 {} JSON 文件失败: {}", kind, file_path);
        return nullptr;
    }

    return std::make_shared<nlohmann::json>(std::move(parsed));
}

inline void clearJsonCache() {
    g_level_json_cache.clear();
    g_level_json_cache_order.clear();
    g_tileset_json_cache.clear();
    g_tileset_json_cache_order.clear();
}

[[nodiscard]] inline std::size_t levelJsonCacheSize() {
    return g_level_json_cache.size();
}

[[nodiscard]] inline std::size_t tilesetJsonCacheSize() {
    return g_tileset_json_cache.size();
}

namespace detail {
inline void enforceCapacity(std::unordered_map<std::string, JsonPtr>& cache,
                            std::deque<std::string>& order,
                            std::size_t max_entries) {
    if (max_entries == 0) {
        return;
    }
    while (cache.size() > max_entries && !order.empty()) {
        cache.erase(order.front());
        order.pop_front();
    }
}

[[nodiscard]] inline JsonPtr getOrLoadJson(std::unordered_map<std::string, JsonPtr>& cache,
                                          std::deque<std::string>& order,
                                          std::size_t max_entries,
                                          std::string_view file_path,
                                          std::string_view kind) {
    const std::string key(file_path);
    if (auto it = cache.find(key); it != cache.end()) {
        return it->second;
    }

    auto json_ptr = loadJsonFromFile(file_path, kind);
    if (!json_ptr) {
        return nullptr;
    }

    cache.emplace(key, json_ptr);
    order.emplace_back(key);
    enforceCapacity(cache, order, max_entries);
    return json_ptr;
}
} // namespace detail

[[nodiscard]] inline JsonPtr getOrLoadLevelJson(std::string_view file_path) {
    return detail::getOrLoadJson(
        g_level_json_cache,
        g_level_json_cache_order,
        LEVEL_JSON_CACHE_MAX_ENTRIES,
        file_path,
        "level"
    );
}

[[nodiscard]] inline JsonPtr getOrLoadTilesetJson(std::string_view file_path) {
    return detail::getOrLoadJson(
        g_tileset_json_cache,
        g_tileset_json_cache_order,
        TILESET_JSON_CACHE_MAX_ENTRIES,
        file_path,
        "tileset"
    );
}

} // namespace engine::loader::tiled
