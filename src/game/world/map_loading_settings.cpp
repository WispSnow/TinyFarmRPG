#include "map_loading_settings.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>

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
} // namespace

MapLoadingSettings MapLoadingSettings::loadFromFile(std::string_view path) {
    MapLoadingSettings settings{};
    settings.source_path = std::string(path);

    if (!std::filesystem::exists(std::filesystem::path(path))) {
        spdlog::warn("MapLoadingSettings: 配置文件不存在，使用默认值: {}", path);
        return settings;
    }

    std::ifstream file{std::filesystem::path(path)};
    if (!file.is_open()) {
        spdlog::warn("MapLoadingSettings: 无法打开配置文件，使用默认值: {}", path);
        return settings;
    }

    nlohmann::json json;
    try {
        file >> json;
    } catch (const std::exception& e) {
        spdlog::warn("MapLoadingSettings: 解析配置失败，使用默认值: {} ({})", path, e.what());
        return settings;
    }

    // preload.mode（优先）
    if (json.contains("preload") && json["preload"].is_object()) {
        const auto& preload = json["preload"];
        settings.preload_mode = parseMode(preload.value("mode", "all"));
    } else {
        // 兼容顶层字段
        settings.preload_mode = parseMode(json.value("mode", "all"));
    }

    settings.log_timings = json.value("log_timings", settings.log_timings);
    return settings;
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
