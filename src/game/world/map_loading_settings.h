#pragma once

#include <cstddef>
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
    bool async_preload_enabled{true};
    std::uint32_t async_wait_budget_ms{3};
    std::uint32_t async_submit_wait_ms{1};
    std::uint32_t async_command_wait_ms{8};
    std::size_t async_worker_count{1};
    std::size_t async_queue_capacity{32};
    std::string source_path{};

    [[nodiscard]] static MapLoadingSettings loadFromFile(std::string_view path);
    [[nodiscard]] static std::string_view toString(MapPreloadMode mode);
};

} // namespace game::world
