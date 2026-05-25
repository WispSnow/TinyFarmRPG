#include "zone_trigger_system.h"

#include "engine/component/tags.h"
#include "engine/component/transform_component.h"
#include "game/component/script_zone_component.h"
#include "game/defs/events_map.h"
#include "game/system/system_helpers.h"
#include "game/world/world_state.h"

#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>

#include <utility>

namespace game::system {

ZoneTriggerSystem::ZoneTriggerSystem(entt::registry& registry,
                                     entt::dispatcher& dispatcher,
                                     game::world::WorldState& world_state)
    : registry_(registry),
      dispatcher_(dispatcher),
      world_state_(world_state) {
}

void ZoneTriggerSystem::update(float delta_time) {
    (void)delta_time;

    const entt::entity player = game::system::helpers::getPlayerEntity(registry_);
    const auto* player_transform =
        player == entt::null ? nullptr : registry_.try_get<engine::component::TransformComponent>(player);
    if (!player_transform) {
        active_zones_.clear();
        return;
    }

    const entt::id_type current_map_id = world_state_.getCurrentMap();
    std::unordered_set<entt::entity> current_zones{};

    auto view = registry_.view<game::component::ScriptZoneComponent>(
        entt::exclude<engine::component::NeedRemoveTag>);
    for (auto zone : view) {
        const auto& script_zone = view.get<game::component::ScriptZoneComponent>(zone);
        if (script_zone.map_id_ != entt::null && current_map_id != entt::null &&
            script_zone.map_id_ != current_map_id) {
            continue;
        }
        if (!script_zone.rect_.contains(player_transform->position_)) {
            continue;
        }

        current_zones.insert(zone);
        if (!active_zones_.contains(zone)) {
            emitEnter(player, zone, script_zone);
        }
    }

    for (const entt::entity zone : active_zones_) {
        if (current_zones.contains(zone)) {
            continue;
        }
        if (!registry_.valid(zone)) {
            continue;
        }
        if (const auto* script_zone = registry_.try_get<game::component::ScriptZoneComponent>(zone)) {
            emitExit(player, zone, *script_zone);
        }
    }

    active_zones_ = std::move(current_zones);
}

std::string ZoneTriggerSystem::mapName(const entt::id_type map_id) const {
    if (map_id == entt::null) {
        return {};
    }
    const auto* state = world_state_.getMapState(map_id);
    return state ? state->info.name : std::string{};
}

void ZoneTriggerSystem::emitEnter(const entt::entity player,
                                  const entt::entity zone,
                                  const game::component::ScriptZoneComponent& script_zone) {
    const entt::id_type map_id = script_zone.map_id_ != entt::null ? script_zone.map_id_ : world_state_.getCurrentMap();
    dispatcher_.trigger(game::defs::ZoneEnteredEvent{
        .player = player,
        .zone = zone,
        .map_id = map_id,
        .map_name = mapName(map_id),
        .zone_id = script_zone.zone_id_,
        .zone_id_hash = script_zone.zone_id_hash_,
    });
}

void ZoneTriggerSystem::emitExit(const entt::entity player,
                                 const entt::entity zone,
                                 const game::component::ScriptZoneComponent& script_zone) {
    const entt::id_type map_id = script_zone.map_id_ != entt::null ? script_zone.map_id_ : world_state_.getCurrentMap();
    dispatcher_.trigger(game::defs::ZoneExitedEvent{
        .player = player,
        .zone = zone,
        .map_id = map_id,
        .map_name = mapName(map_id),
        .zone_id = script_zone.zone_id_,
        .zone_id_hash = script_zone.zone_id_hash_,
    });
}

} // namespace game::system
