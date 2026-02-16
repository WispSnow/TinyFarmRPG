#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace game::world {

enum class MapPreloadMode : std::uint8_t {
    Off = 0,
    Neighbors = 1,
    All = 2,
};

struct MapLoadingSettings {
    MapPreloadMode preload_mode{MapPreloadMode::All};
    bool log_timings{false};
    std::string source_path{};

    [[nodiscard]] static MapLoadingSettings loadFromFile(std::string_view path);
    [[nodiscard]] static std::string_view toString(MapPreloadMode mode);
};

} // namespace game::world
