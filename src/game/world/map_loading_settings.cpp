#include "map_loading_settings.h"

#include "engine/platform/threading.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <limits>
#include <utility>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace game::world {

namespace {
[[nodiscard]] std::string toLower(std::string value) {
    std::ranges::transform(value, value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

[[nodiscard]] MapPreloadMode parseMode(std::string_view mode) {
    const std::string m = toLower(std::string(mode));
    if (m == "off" || m == "false" || m == "0") {
        return MapPreloadMode::Off;
    }
    if (m == "neighbors" || m == "neighbour" || m == "neighbor") {
        return MapPreloadMode::Neighbors;
    }
    if (m == "all" || m == "true" || m == "1") {
        return MapPreloadMode::All;
    }
    spdlog::warn("MapLoadingSettings: 未知 preload.mode='{}'，回退为 off", mode);
    return MapPreloadMode::Off;
}

[[nodiscard]] const nlohmann::json* findMember(const nlohmann::json& json, std::string_view key) {
    if (!json.is_object()) {
        return nullptr;
    }
    const auto it = json.find(std::string(key));
    if (it == json.end()) {
        return nullptr;
    }
    return &(*it);
}

[[nodiscard]] std::string stringOr(const nlohmann::json& json, std::string_view key, std::string_view fallback) {
    const auto* value = findMember(json, key);
    if (!value) {
        return std::string(fallback);
    }
    if (const auto* text = value->get_ptr<const nlohmann::json::string_t*>()) {
        return *text;
    }
    return std::string(fallback);
}

[[nodiscard]] bool boolOr(const nlohmann::json& json, std::string_view key, bool fallback) {
    const auto* value = findMember(json, key);
    if (!value) {
        return fallback;
    }
    if (const auto* flag = value->get_ptr<const nlohmann::json::boolean_t*>()) {
        return *flag;
    }
    return fallback;
}

template <typename T>
[[nodiscard]] T parseUnsigned(const nlohmann::json& json,
                              std::string_view key,
                              T fallback,
                              T max_value = std::numeric_limits<T>::max()) {
    const auto* value = findMember(json, key);
    if (!value) {
        return fallback;
    }

    using JsonUnsigned = nlohmann::json::number_unsigned_t;
    const auto max_unsigned = static_cast<JsonUnsigned>(max_value);

    if (const auto* unsigned_value = value->get_ptr<const JsonUnsigned*>()) {
        if (*unsigned_value > max_unsigned) {
            return max_value;
        }
        return static_cast<T>(*unsigned_value);
    }
    if (const auto* signed_value = value->get_ptr<const nlohmann::json::number_integer_t*>()) {
        if (*signed_value < 0) {
            return fallback;
        }
        const auto unsigned_value = static_cast<JsonUnsigned>(*signed_value);
        if (unsigned_value > max_unsigned) {
            return max_value;
        }
        return static_cast<T>(unsigned_value);
    }
    return fallback;
}
} // namespace

MapLoadingSettings MapLoadingSettings::forCurrentPlatform(MapLoadingSettings settings) {
#ifdef TF_WEB_DIRECT_MAP_BOOT
    settings.preload_mode = MapPreloadMode::Off;
#endif
    if (!engine::platform::runtimeThreadingEnabled()) {
        settings.async_preload_enabled = false;
        settings.async_wait_budget_ms = 0;
        settings.async_submit_wait_ms = 0;
        settings.async_command_wait_ms = 0;
        settings.async_worker_count = 0;
        settings.async_queue_capacity = 0;
    }
    return settings;
}

MapLoadingSettings MapLoadingSettings::loadFromFile(std::string_view path) {
    MapLoadingSettings settings{};
    settings.source_path = std::string(path);

    std::ifstream file{std::string(path)};
    if (!file.is_open()) {
        spdlog::warn("MapLoadingSettings: 无法打开配置文件，使用默认值: {}", path);
        return forCurrentPlatform(std::move(settings));
    }

    const std::string file_content(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>()
    );

    const nlohmann::json json = nlohmann::json::parse(file_content, nullptr, false);
    if (json.is_discarded() || !json.is_object()) {
        spdlog::warn("MapLoadingSettings: 解析配置失败，使用默认值: {}", path);
        return forCurrentPlatform(std::move(settings));
    }

    // preload.mode（优先）
    if (const auto* preload = findMember(json, "preload"); preload && preload->is_object()) {
        settings.preload_mode = parseMode(stringOr(*preload, "mode", "all"));
        settings.async_preload_enabled = boolOr(*preload, "async_enabled", settings.async_preload_enabled);
        settings.async_wait_budget_ms = parseUnsigned<std::uint32_t>(
            *preload, "async_wait_budget_ms", settings.async_wait_budget_ms, 2000U);
        settings.async_submit_wait_ms = parseUnsigned<std::uint32_t>(
            *preload, "async_submit_wait_ms", settings.async_submit_wait_ms, 2000U);
        settings.async_command_wait_ms = parseUnsigned<std::uint32_t>(
            *preload, "async_command_wait_ms", settings.async_command_wait_ms, 2000U);
        settings.async_worker_count = parseUnsigned<std::size_t>(
            *preload, "async_worker_count", settings.async_worker_count, 64U);
        settings.async_queue_capacity = parseUnsigned<std::size_t>(
            *preload, "async_queue_capacity", settings.async_queue_capacity, 4096U);
    } else {
        // 兼容顶层字段
        settings.preload_mode = parseMode(stringOr(json, "mode", "all"));
    }

    settings.log_timings = boolOr(json, "log_timings", settings.log_timings);
    return forCurrentPlatform(std::move(settings));
}

std::string_view MapLoadingSettings::toString(MapPreloadMode mode) {
    switch (mode) {
        case MapPreloadMode::Off:
            return "off";
        case MapPreloadMode::Neighbors:
            return "neighbors";
        case MapPreloadMode::All:
            return "all";
    }
    return "all";
}

} // namespace game::world
