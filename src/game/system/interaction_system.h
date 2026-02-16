#pragma once

#include <entt/entity/entity.hpp>
#include <entt/entity/fwd.hpp>
#include <entt/signal/fwd.hpp>

namespace engine::input {
class InputManager;
}

namespace engine::spatial {
class SpatialIndexManager;
}

namespace game::world {
class WorldState;
}

namespace game::system {

class InteractionSystem final {
    entt::registry& registry_;
    entt::dispatcher& dispatcher_;
    engine::input::InputManager& input_manager_;
    engine::spatial::SpatialIndexManager& spatial_index_manager_;
    game::world::WorldState& world_state_;

public:
    InteractionSystem(entt::registry& registry,
                      entt::dispatcher& dispatcher,
                      engine::input::InputManager& input_manager,
                      engine::spatial::SpatialIndexManager& spatial_index_manager,
                      game::world::WorldState& world_state);

    void update();

private:
    [[nodiscard]] entt::entity findActiveDialogueTarget(entt::id_type current_map) const;
    [[nodiscard]] entt::entity chooseFacingTarget(entt::entity player, entt::id_type current_map) const;
};

} // namespace game::system
