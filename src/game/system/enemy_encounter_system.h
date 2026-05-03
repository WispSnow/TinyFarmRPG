#pragma once

#include <entt/core/fwd.hpp>
#include <entt/entity/fwd.hpp>
#include <entt/signal/fwd.hpp>

namespace engine::spatial {
class SpatialIndexManager;
}

namespace game::data {
class RpgCatalog;
}

namespace game::world {
class WorldState;
}

namespace game::system {

class EnemyEncounterSystem final {
    entt::registry& registry_;
    entt::dispatcher& dispatcher_;
    engine::spatial::SpatialIndexManager& spatial_index_manager_;
    game::world::WorldState& world_state_;
    const game::data::RpgCatalog* rpg_catalog_{nullptr};

public:
    EnemyEncounterSystem(entt::registry& registry,
                         entt::dispatcher& dispatcher,
                         engine::spatial::SpatialIndexManager& spatial_index_manager,
                         game::world::WorldState& world_state,
                         const game::data::RpgCatalog* rpg_catalog);

    void update(float delta_time);

private:
    void tickCooldowns(float delta_time);
    [[nodiscard]] bool hasEngagedEncounter(entt::id_type current_map) const;
    [[nodiscard]] bool isValidEncounterTarget(entt::entity entity, entt::id_type current_map) const;
};

} // namespace game::system
