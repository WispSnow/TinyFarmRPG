#pragma once

#include "engine/debug/debug_panel.h"

#include <entt/entity/fwd.hpp>

namespace engine::spatial {
class SpatialIndexManager;
}

namespace game::world {
class MapManager;
class WorldState;
}

namespace game::debug {

class MapInspectorDebugPanel final : public engine::debug::DebugPanel {
    const game::world::MapManager& map_manager_;
    const game::world::WorldState& world_state_;
    entt::registry& registry_;
    engine::spatial::SpatialIndexManager& spatial_index_manager_;

public:
    MapInspectorDebugPanel(const game::world::MapManager& map_manager,
                           const game::world::WorldState& world_state,
                           entt::registry& registry,
                           engine::spatial::SpatialIndexManager& spatial_index_manager);

    std::string_view name() const override;
    void draw(bool& is_open) override;
};

} // namespace game::debug

