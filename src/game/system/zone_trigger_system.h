#pragma once

#include <entt/entity/entity.hpp>
#include <entt/entity/fwd.hpp>
#include <entt/signal/fwd.hpp>

#include <string>
#include <unordered_set>

namespace game::component {
struct ScriptZoneComponent;
}

namespace game::world {
class WorldState;
}

namespace game::system {

class ZoneTriggerSystem final {
public:
    ZoneTriggerSystem(entt::registry& registry,
                      entt::dispatcher& dispatcher,
                      game::world::WorldState& world_state);

    void update(float delta_time);

private:
    [[nodiscard]] std::string mapName(entt::id_type map_id) const;
    void emitEnter(entt::entity player, entt::entity zone, const game::component::ScriptZoneComponent& script_zone);
    void emitExit(entt::entity player, entt::entity zone, const game::component::ScriptZoneComponent& script_zone);

    entt::registry& registry_;
    entt::dispatcher& dispatcher_;
    game::world::WorldState& world_state_;
    std::unordered_set<entt::entity> active_zones_{};
};

} // namespace game::system
