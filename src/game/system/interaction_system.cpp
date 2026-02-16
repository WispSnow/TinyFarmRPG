#include "interaction_system.h"
#include "system_helpers.h"

#include "game/component/chest_component.h"
#include "game/component/map_component.h"
#include "game/component/npc_component.h"
#include "game/component/state_component.h"
#include "game/component/tags.h"
#include "game/defs/constants.h"
#include "game/defs/events.h"
#include "game/defs/spatial_layers.h"
#include "game/world/world_state.h"
#include "engine/component/transform_component.h"
#include "engine/input/input_manager.h"
#include "engine/spatial/spatial_index_manager.h"

#include <entt/core/hashed_string.hpp>
#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>
#include <glm/geometric.hpp>

#include <algorithm>
#include <limits>

using namespace entt::literals;

namespace {
constexpr float PROBE_PADDING_PX = 4.0f;

glm::ivec2 toOffset(game::component::Direction direction) {
    switch (direction) {
        case game::component::Direction::Up: return glm::ivec2(0, -1);
        case game::component::Direction::Down: return glm::ivec2(0, 1);
        case game::component::Direction::Left: return glm::ivec2(-1, 0);
        case game::component::Direction::Right: return glm::ivec2(1, 0);
        case game::component::Direction::Count: break;
    }
    return glm::ivec2(0, 1);
}

bool rectOutsideBounds(const engine::utils::Rect& rect, glm::vec2 bounds_size) {
    if (bounds_size.x <= 0.0f || bounds_size.y <= 0.0f) return false;
    const glm::vec2 max = rect.pos + rect.size;
    return rect.pos.x < 0.0f || rect.pos.y < 0.0f || max.x > bounds_size.x || max.y > bounds_size.y;
}

bool clampRectToBounds(engine::utils::Rect& rect, glm::vec2 bounds_size) {
    if (bounds_size.x <= 0.0f || bounds_size.y <= 0.0f) return true;
    const float min_x = std::max(0.0f, rect.pos.x);
    const float min_y = std::max(0.0f, rect.pos.y);
    const float max_x = std::min(bounds_size.x, rect.pos.x + rect.size.x);
    const float max_y = std::min(bounds_size.y, rect.pos.y + rect.size.y);
    if (max_x <= min_x || max_y <= min_y) {
        return false;
    }
    rect.pos = glm::vec2(min_x, min_y);
    rect.size = glm::vec2(max_x - min_x, max_y - min_y);
    return true;
}
} // namespace

namespace game::system {

InteractionSystem::InteractionSystem(entt::registry& registry,
                                     entt::dispatcher& dispatcher,
                                     engine::input::InputManager& input_manager,
                                     engine::spatial::SpatialIndexManager& spatial_index_manager,
                                     game::world::WorldState& world_state)
    : registry_(registry),
      dispatcher_(dispatcher),
      input_manager_(input_manager),
      spatial_index_manager_(spatial_index_manager),
      world_state_(world_state) {
}

void InteractionSystem::update() {
    if (!input_manager_.isActionPressed("interact"_hs)) {
        return;
    }

    const entt::entity player = helpers::getPlayerEntity(registry_);
    if (player == entt::null) {
        return;
    }

    const entt::id_type current_map = world_state_.getCurrentMap();
    if (auto target = findActiveDialogueTarget(current_map); target != entt::null) {
        dispatcher_.trigger(game::defs::InteractRequest{player, target});
        return;
    }

    if (auto target = chooseFacingTarget(player, current_map); target != entt::null) {
        dispatcher_.trigger(game::defs::InteractRequest{player, target});
    }
}

entt::entity InteractionSystem::findActiveDialogueTarget(entt::id_type current_map) const {
    auto view = registry_.view<game::component::DialogueComponent, game::component::MapId>();
    for (auto entity : view) {
        const auto& map = view.get<game::component::MapId>(entity);
        if (map.id_ != current_map) continue;
        const auto& dialogue = view.get<game::component::DialogueComponent>(entity);
        if (dialogue.active_) {
            return entity;
        }
    }
    return entt::null;
}

entt::entity InteractionSystem::chooseFacingTarget(entt::entity player, entt::id_type current_map) const {
    if (player == entt::null) return entt::null;

    const auto* player_transform = registry_.try_get<engine::component::TransformComponent>(player);
    const auto* player_state = registry_.try_get<game::component::StateComponent>(player);
    if (!player_transform || !player_state) return entt::null;

    // 1) Build a probe rectangle in front of the player (one-tile probe + padding).
    const glm::vec2 bounds_size = [&]() -> glm::vec2 {
        if (const auto* map = world_state_.getMapState(current_map)) {
            return glm::vec2(map->info.size_px);
        }
        return glm::vec2(0.0f);
    }();

    const glm::ivec2 offset = toOffset(player_state->direction_);
    const glm::vec2 probe_world_pos = player_transform->position_ + glm::vec2(offset) * game::defs::TILE_SIZE;
    engine::utils::Rect probe_rect = spatial_index_manager_.getRectAtWorldPos(probe_world_pos);
    if (rectOutsideBounds(probe_rect, bounds_size)) {
        return entt::null;
    }

    probe_rect.pos -= glm::vec2(PROBE_PADDING_PX);
    probe_rect.size += glm::vec2(PROBE_PADDING_PX * 2.0f);
    // Padding near map edges may slightly exceed bounds; clamp to keep spatial queries safe.
    if (!clampRectToBounds(probe_rect, bounds_size)) {
        return entt::null;
    }

    // 2) Query spatial index for candidates overlapped by the probe.
    const auto collision = spatial_index_manager_.checkCollision(probe_rect);
    const auto& candidates = collision.dynamic_colliders;

    // 3) Pick best target by priority: Dialogue NPC > Chest > Rest tile.
    entt::entity best_npc = entt::null;
    float best_npc_distance = std::numeric_limits<float>::max();
    entt::entity best_chest = entt::null;
    float best_chest_distance = std::numeric_limits<float>::max();

    for (const auto entity : candidates) {
        if (entity == player) continue;
        if (!registry_.valid(entity)) continue;

        if (auto* map = registry_.try_get<game::component::MapId>(entity); map && map->id_ != current_map) {
            continue;
        }

        const auto* transform = registry_.try_get<engine::component::TransformComponent>(entity);
        if (!transform) continue;

        const float distance = glm::distance(transform->position_, player_transform->position_);

        if (auto* dialogue = registry_.try_get<game::component::DialogueComponent>(entity)) {
            if (dialogue->dialogue_id_ == entt::null) continue;
            if (dialogue->cooldown_timer_ > 0.0f) continue;
            if (auto* sleep = registry_.try_get<game::component::SleepRoutine>(entity); sleep && sleep->is_sleeping_) continue;
            if (distance < best_npc_distance) {
                best_npc_distance = distance;
                best_npc = entity;
            }
            continue;
        }

        if (auto* chest = registry_.try_get<game::component::ChestComponent>(entity)) {
            if (chest->opened_) continue;
            if (distance < best_chest_distance) {
                best_chest_distance = distance;
                best_chest = entity;
            }
            continue;
        }
    }

    if (best_npc == entt::null && best_chest == entt::null) {
        if (auto rest = spatial_index_manager_.getTileEntityAtWorldPos(probe_world_pos, game::defs::spatial_layer::REST);
            rest != entt::null && registry_.valid(rest)) {
            if (auto* map = registry_.try_get<game::component::MapId>(rest); map && map->id_ != current_map) {
                return entt::null;
            }
            return rest;
        }
    }

    return best_npc != entt::null ? best_npc : best_chest;
}

} // namespace game::system
