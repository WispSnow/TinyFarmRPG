#include "game/system/enemy_encounter_system.h"

#include "engine/component/transform_component.h"
#include "engine/spatial/collider_shape.h"
#include "engine/spatial/spatial_index_manager.h"
#include "engine/utils/math.h"
#include "game/component/enemy_encounter_component.h"
#include "game/component/map_component.h"
#include "game/data/rpg_catalog.h"
#include "game/defs/commands.h"
#include "game/system/system_helpers.h"
#include "game/world/world_state.h"

#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>
#include <glm/vec2.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <vector>

namespace {

constexpr float TOUCH_PADDING_PX = 4.0F;
constexpr float INVALID_TROOP_COOLDOWN_SECONDS = 1.0F;

[[nodiscard]] engine::utils::Rect inflatedRect(const engine::utils::Rect& rect, float padding) {
    return engine::utils::Rect(rect.pos - glm::vec2{padding, padding},
                               rect.size + glm::vec2{padding * 2.0F, padding * 2.0F});
}

[[nodiscard]] std::vector<entt::entity> queryTouchingColliders(
    const engine::spatial::SpatialIndexManager& spatial_index_manager,
    const engine::spatial::ColliderShape& player_shape) {
    if (player_shape.type == engine::spatial::ColliderShapeType::Circle) {
        return spatial_index_manager.queryColliders(player_shape.center, player_shape.radius + TOUCH_PADDING_PX);
    }
    return spatial_index_manager.queryColliders(inflatedRect(player_shape.rect, TOUCH_PADDING_PX));
}

} // namespace

namespace game::system {

EnemyEncounterSystem::EnemyEncounterSystem(entt::registry& registry,
                                           entt::dispatcher& dispatcher,
                                           engine::spatial::SpatialIndexManager& spatial_index_manager,
                                           game::world::WorldState& world_state,
                                           const game::data::RpgCatalog* rpg_catalog)
    : registry_(registry),
      dispatcher_(dispatcher),
      spatial_index_manager_(spatial_index_manager),
      world_state_(world_state),
      rpg_catalog_(rpg_catalog) {}

void EnemyEncounterSystem::update(float delta_time) {
    tickCooldowns(delta_time);

    if (!spatial_index_manager_.isInitialized()) {
        return;
    }

    const entt::id_type current_map = world_state_.getCurrentMap();
    if (current_map == entt::null || hasEngagedEncounter(current_map)) {
        return;
    }

    const entt::entity player = helpers::getPlayerEntity(registry_);
    if (player == entt::null) {
        return;
    }

    const auto* player_map = registry_.try_get<game::component::MapId>(player);
    if (!player_map || player_map->id_ != current_map) {
        return;
    }

    const auto player_shape = engine::spatial::buildColliderShape(registry_, player);
    if (!player_shape) {
        return;
    }

    for (const entt::entity candidate : queryTouchingColliders(spatial_index_manager_, *player_shape)) {
        if (candidate == player || !isValidEncounterTarget(candidate, current_map)) {
            continue;
        }

        auto& encounter = registry_.get<game::component::EnemyEncounterComponent>(candidate);
        if (rpg_catalog_ && !rpg_catalog_->findTroop(encounter.troop_id_hash_)) {
            spdlog::warn("EnemyEncounterSystem: encounter_id={} 引用的 battle_troop_id='{}' 未在 RpgCatalog 中找到。",
                         encounter.encounter_id_,
                         encounter.troop_id_);
            encounter.cooldown_timer_ = std::max(encounter.cooldown_timer_, INVALID_TROOP_COOLDOWN_SECONDS);
            continue;
        }

        encounter.engaged_ = true;

        game::defs::EnterBattleCommand command{};
        command.troop_id = encounter.troop_id_;
        command.encounter_context = game::defs::EnemyEncounterBattleContext{
            candidate,
            current_map,
            encounter.encounter_id_,
            encounter.troop_id_,
            encounter.once_,
            encounter.home_position_,
        };
        dispatcher_.trigger(command);
        return;
    }
}

void EnemyEncounterSystem::tickCooldowns(float delta_time) {
    if (delta_time <= 0.0F) {
        return;
    }

    auto view = registry_.view<game::component::EnemyEncounterComponent>();
    for (const entt::entity entity : view) {
        auto& encounter = view.get<game::component::EnemyEncounterComponent>(entity);
        if (encounter.cooldown_timer_ <= 0.0F) {
            continue;
        }
        encounter.cooldown_timer_ = std::max(0.0F, encounter.cooldown_timer_ - delta_time);
    }
}

bool EnemyEncounterSystem::hasEngagedEncounter(entt::id_type current_map) const {
    auto view = registry_.view<game::component::EnemyEncounterComponent, game::component::MapId>();
    for (const entt::entity entity : view) {
        const auto& [encounter, map] = view.get<game::component::EnemyEncounterComponent, game::component::MapId>(entity);
        if (map.id_ == current_map && encounter.engaged_) {
            return true;
        }
    }
    return false;
}

bool EnemyEncounterSystem::isValidEncounterTarget(entt::entity entity, entt::id_type current_map) const {
    if (entity == entt::null || !registry_.valid(entity)) {
        return false;
    }

    const auto* map = registry_.try_get<game::component::MapId>(entity);
    const auto* encounter = registry_.try_get<game::component::EnemyEncounterComponent>(entity);
    if (!map || !encounter) {
        return false;
    }

    return map->id_ == current_map &&
           !encounter->defeated_ &&
           !encounter->engaged_ &&
           encounter->cooldown_timer_ <= 0.0F &&
           encounter->encounter_id_ > 0 &&
           encounter->troop_id_hash_ != entt::null &&
           !encounter->troop_id_.empty();
}

} // namespace game::system
