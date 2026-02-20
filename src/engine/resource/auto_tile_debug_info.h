#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <entt/core/fwd.hpp>

namespace engine::resource {

enum class AutoTileTopology : std::uint8_t {
    CORNER,
    EDGE,
    MIXED,
    UNKNOWN,
};

struct AutoTileRuleDebugInfo {
    entt::id_type rule_id{};
    std::string name;
    entt::id_type texture_id{};
    AutoTileTopology topology{AutoTileTopology::UNKNOWN};
    std::size_t defined_mask_count{0};
    std::size_t manual_mask_count{0};
    std::size_t missing_mask_count{0};
    std::vector<std::uint8_t> missing_masks;
};

} // namespace engine::resource
