#include "map_transition_system.h"

#include "game/world/world_state.h"
#include "game/world/map_manager.h"
#include "game/component/tags.h"
#include "game/component/map_component.h"
#include "engine/component/transform_component.h"
#include "engine/component/tags.h"
#include "engine/component/collider_component.h"
#include "engine/spatial/collision_resolver.h"
#include "engine/ui/ui_screen_fade.h"
#include <entt/entity/registry.hpp>
#include <spdlog/spdlog.h>
#include <algorithm>

using namespace entt::literals;

namespace game::system {

MapTransitionSystem::MapTransitionSystem(entt::registry& registry,
                                         game::world::WorldState& world_state,
                                         game::world::MapManager& map_manager,
                                         engine::spatial::CollisionResolver* collision_resolver,
                                         float edge_offset)
    : registry_(registry),
      world_state_(world_state),
      map_manager_(map_manager),
      collision_resolver_(collision_resolver),
      edge_offset_(edge_offset) {
}

void MapTransitionSystem::update() {
    if (transition_phase_ != TransitionPhase::Idle) {
        updateTransition();
        return;
    }

    auto view = registry_.view<game::component::PlayerTag, engine::component::TransformComponent>();
    if (view.begin() == view.end()) return;

    entt::entity player = *view.begin();
    auto& transform = view.get<engine::component::TransformComponent>(player);
    const glm::vec2 pos = transform.position_;

    if (handleEdgeTransition(player, pos)) {
        return;
    }
    handleTriggerTransition(player, pos);
}

void MapTransitionSystem::beginTransition(PendingTransition pending) {
    if (transition_phase_ != TransitionPhase::Idle) {
        return;
    }
    if (!registry_.valid(pending.player) || pending.target_map_id == entt::null) {
        return;
    }

    pending_ = pending;
    transition_phase_ = TransitionPhase::FadingOut;
    registry_.emplace_or_replace<game::component::ActionLockedTag>(pending_.player);

    if (fade_) {
        fade_->fadeOut(fade_seconds_);
        return;
    }

    (void)commitPendingTransition();
    finishTransition();
}

void MapTransitionSystem::updateTransition() {
    if (transition_phase_ == TransitionPhase::Idle) {
        return;
    }

    if (!fade_) {
        finishTransition();
        return;
    }

    if (transition_phase_ == TransitionPhase::FadingOut) {
        if (fade_->phase() != engine::ui::UIScreenFade::Phase::Holding) {
            return;
        }

        (void)commitPendingTransition();
        fade_->fadeIn(fade_seconds_);
        transition_phase_ = TransitionPhase::FadingIn;
        return;
    }

    if (transition_phase_ == TransitionPhase::FadingIn) {
        if (fade_->phase() != engine::ui::UIScreenFade::Phase::Idle) {
            return;
        }

        finishTransition();
    }
}

void MapTransitionSystem::finishTransition() {
    if (registry_.valid(pending_.player) && registry_.all_of<game::component::ActionLockedTag>(pending_.player)) {
        registry_.remove<game::component::ActionLockedTag>(pending_.player);
    }

    pending_ = PendingTransition{};
    transition_phase_ = TransitionPhase::Idle;
}

bool MapTransitionSystem::commitPendingTransition() {
    if (!registry_.valid(pending_.player) || pending_.target_map_id == entt::null) {
        spdlog::warn("MapTransitionSystem: pending transition 无效，取消提交");
        return false;
    }

    if (!map_manager_.loadMap(pending_.target_map_id)) {
        spdlog::error("MapTransitionSystem: 切换地图失败 (map_id={})", pending_.target_map_id);
        return false;
    }

    glm::vec2 spawn_pos{0.0f, 0.0f};

    if (pending_.type == TransitionType::Edge) {
        spawn_pos = pending_.edge_spawn_pos;
    } else if (pending_.type == TransitionType::Trigger) {
        glm::vec2 fallback = pending_.fallback_spawn_pos;
        glm::vec2 target_size = map_manager_.currentMapPixelSize();

        if (const auto* state = world_state_.getMapState(pending_.target_map_id)) {
            target_size = glm::vec2(state->info.size_px);
        }
        if (target_size != glm::vec2(0.0f)) {
            fallback = clampToMap(fallback, target_size);
        }

        spawn_pos = fallback;

        auto target_view = registry_.view<game::component::MapTrigger>();
        for (auto target : target_view) {
            const auto& target_trigger = target_view.get<game::component::MapTrigger>(target);
            if (target_trigger.map_id_ != pending_.target_map_id) continue;
            if (target_trigger.self_id_ != pending_.target_trigger_id) continue;
            spawn_pos = computeOffsetPosition(target_trigger.rect_, target_trigger.start_offset_);
            break;
        }
    } else {
        spdlog::warn("MapTransitionSystem: 未知 transition 类型，使用地图中心作为落点");
        spawn_pos = map_manager_.currentMapPixelSize() * 0.5f;
    }

    spawn_pos = findSafeSpawnPosition(pending_.player, spawn_pos);

    auto& transform = registry_.get<engine::component::TransformComponent>(pending_.player);
    transform.position_ = spawn_pos;
    registry_.emplace_or_replace<engine::component::TransformDirtyTag>(pending_.player);
    map_manager_.snapCameraTo(spawn_pos);
    return true;
}

bool MapTransitionSystem::handleEdgeTransition(entt::entity player, const glm::vec2& pos) {
    const entt::id_type current_map = map_manager_.currentMapId();
    if (current_map == entt::null) return false;
    const auto* map_state = world_state_.getMapState(current_map);
    if (!map_state) return false;

    const glm::vec2 map_size = glm::vec2(map_state->info.size_px);
    engine::spatial::Direction dir = engine::spatial::Direction::North;
    bool out_of_bounds = false;

    // 获取碰撞盒边界
    glm::vec2 bounds_min = pos;
    glm::vec2 bounds_max = pos;

    if (auto* aabb = registry_.try_get<engine::component::AABBCollider>(player)) {
        bounds_min = pos + aabb->offset_ - aabb->size_ * 0.5f;
        bounds_max = pos + aabb->offset_ + aabb->size_ * 0.5f;
    } else if (auto* circle = registry_.try_get<engine::component::CircleCollider>(player)) {
        bounds_min = pos + circle->offset_ - glm::vec2(circle->radius_);
        bounds_max = pos + circle->offset_ + glm::vec2(circle->radius_);
    }

    if (bounds_min.x < 0.0f) {
        dir = engine::spatial::Direction::West;
        out_of_bounds = true;
    } else if (bounds_max.x > map_size.x) {
        dir = engine::spatial::Direction::East;
        out_of_bounds = true;
    } else if (bounds_min.y < 0.0f) {
        dir = engine::spatial::Direction::North;
        out_of_bounds = true;
    } else if (bounds_max.y > map_size.y) {
        dir = engine::spatial::Direction::South;
        out_of_bounds = true;
    }

    if (!out_of_bounds) return false;

    auto neighbor_opt = world_state_.getNeighbor(current_map, dir);
    if (!neighbor_opt) {
        // 没有邻接地图：将玩家（按 collider bounds）约束回当前地图范围内。
        float min_local_x = bounds_min.x - pos.x;
        float max_local_x = bounds_max.x - pos.x;
        float min_local_y = bounds_min.y - pos.y;
        float max_local_y = bounds_max.y - pos.y;

        auto& transform = registry_.get<engine::component::TransformComponent>(player);
        transform.position_.x = std::clamp(pos.x, -min_local_x, map_size.x - max_local_x);
        transform.position_.y = std::clamp(pos.y, -min_local_y, map_size.y - max_local_y);
        
        registry_.emplace_or_replace<engine::component::TransformDirtyTag>(player);
        return true;
    }

    const auto* neighbor_state = world_state_.getMapState(*neighbor_opt);
    if (!neighbor_state) {
        spdlog::warn("MapTransitionSystem: 找到邻接ID但缺少 MapState");
        return false;
    }

    const glm::vec2 target_size = glm::vec2(neighbor_state->info.size_px);
    glm::vec2 new_pos = computeEdgeSpawnPos(dir, pos, target_size);

    if (!fade_) {
        if (!map_manager_.loadMap(neighbor_state->info.id)) {
            spdlog::error("MapTransitionSystem: 切换到邻接地图失败");
            return false;
        }

        new_pos = findSafeSpawnPosition(player, new_pos);

        auto& transform = registry_.get<engine::component::TransformComponent>(player);
        transform.position_ = new_pos;
        registry_.emplace_or_replace<engine::component::TransformDirtyTag>(player);
        map_manager_.snapCameraTo(new_pos);
        return true;
    }

    PendingTransition pending{};
    pending.type = TransitionType::Edge;
    pending.player = player;
    pending.target_map_id = neighbor_state->info.id;
    pending.edge_spawn_pos = new_pos;
    beginTransition(pending);
    return true;
}

glm::vec2 MapTransitionSystem::computeEdgeSpawnPos(engine::spatial::Direction dir,
                                                   const glm::vec2& pos,
                                                   const glm::vec2& target_size) const {
    // Direction convention (screen-space):
    //
    //        North (y < 0)
    //           ^
    // West <----+----> East
    //           v
    //        South (y > 0)
    //
    // Edge transition rule: keep the non-transition axis (clamped) and push the player `edge_offset_` px inside.
    switch (dir) {
        case engine::spatial::Direction::East:
            return {edge_offset_, std::clamp(pos.y, 0.0f, target_size.y)};
        case engine::spatial::Direction::West:
            return {target_size.x - edge_offset_, std::clamp(pos.y, 0.0f, target_size.y)};
        case engine::spatial::Direction::North:
            return {std::clamp(pos.x, 0.0f, target_size.x), target_size.y - edge_offset_};
        case engine::spatial::Direction::South:
            return {std::clamp(pos.x, 0.0f, target_size.x), edge_offset_};
    }
    return pos;
}

bool MapTransitionSystem::handleTriggerTransition(entt::entity player, const glm::vec2& pos) {
    const entt::id_type current_map = map_manager_.currentMapId();
    if (current_map == entt::null) return false;

    auto view = registry_.view<game::component::MapTrigger>();
    for (auto entity : view) {
        const auto& trigger = view.get<game::component::MapTrigger>(entity);
        if (trigger.map_id_ != current_map) continue;

        const auto& rect = trigger.rect_;
        if (!rect.contains(pos)) continue;
        entt::id_type target_map_id = trigger.target_map_;
        if (!world_state_.getMapState(target_map_id) && !trigger.target_map_name_.empty()) {
            target_map_id = world_state_.ensureExternalMap(trigger.target_map_name_);
        }
        if (target_map_id == entt::null) continue;

        if (!fade_) {
            if (!map_manager_.loadMap(target_map_id)) {
                spdlog::error("MapTransitionSystem: 通过触发器切换地图失败");
                return false;
            }

            glm::vec2 spawn_pos = pos;
            auto target_view = registry_.view<game::component::MapTrigger>();
            for (auto target : target_view) {
                const auto& target_trigger = target_view.get<game::component::MapTrigger>(target);
                if (target_trigger.map_id_ != target_map_id) continue;
                if (target_trigger.self_id_ != trigger.target_id_) continue;
                spawn_pos = computeOffsetPosition(target_trigger.rect_, target_trigger.start_offset_);
                break;
            }

            spawn_pos = findSafeSpawnPosition(player, spawn_pos);

            auto& transform = registry_.get<engine::component::TransformComponent>(player);
            transform.position_ = spawn_pos;
            registry_.emplace_or_replace<engine::component::TransformDirtyTag>(player);
            map_manager_.snapCameraTo(spawn_pos);
            return true;
        }

        PendingTransition pending{};
        pending.type = TransitionType::Trigger;
        pending.player = player;
        pending.target_map_id = target_map_id;
        pending.target_trigger_id = trigger.target_id_;
        pending.fallback_spawn_pos = pos;
        beginTransition(pending);
        return true;
    }
    return false;
}

glm::vec2 MapTransitionSystem::findSafeSpawnPosition(entt::entity player, glm::vec2 target_pos) {
    if (!collision_resolver_) {
        return target_pos;
    }
    
    // Check if the exact target position is safe
    if (collision_resolver_->canMoveTo(player, target_pos)) {
        return target_pos;
    }

    spdlog::info("MapTransitionSystem: 目标出生点阻塞，寻找附近安全位置...");

    // Spiral search for a nearby safe tile
    // 假设 16px tile，搜索半径 3 格
    const float step_size = 16.0f;
    const int search_radius = 3; 

    // dx, dy offsets in spiral order or simple grid search
    for (int r = 1; r <= search_radius; ++r) {
        for (int x = -r; x <= r; ++x) {
            for (int y = -r; y <= r; ++y) {
                // Determine if this (x, y) is on the ring 'r'
                if (std::abs(x) != r && std::abs(y) != r) continue;

                glm::vec2 offset(x * step_size, y * step_size);
                glm::vec2 candidate = target_pos + offset;

                // 基本边界检查；上限由碰撞解析器的空间索引隐式约束
                if (candidate.x < 0.0f || candidate.y < 0.0f) continue;

                if (collision_resolver_->canMoveTo(player, candidate)) {
                    spdlog::info("MapTransitionSystem: 找到安全位置 offset ({}, {})", offset.x, offset.y);
                    return candidate;
                }
            }
        }
    }

    spdlog::warn("MapTransitionSystem: 未能在附近找到安全位置，强制使用目标位置");
    return target_pos;
}

glm::vec2 MapTransitionSystem::computeOffsetPosition(const engine::utils::Rect& rect, game::component::StartOffset offset) const {
    const glm::vec2 center = rect.pos + rect.size * 0.5f;
    switch (offset) {
        case game::component::StartOffset::Left:
            return {rect.pos.x - edge_offset_, center.y};
        case game::component::StartOffset::Right:
            return {rect.pos.x + rect.size.x + edge_offset_, center.y};
        case game::component::StartOffset::Top:
            return {center.x, rect.pos.y - edge_offset_};
        case game::component::StartOffset::Bottom:
            return {center.x, rect.pos.y + rect.size.y + edge_offset_};
        case game::component::StartOffset::None:
        default:
            return center;
    }
}

glm::vec2 MapTransitionSystem::clampToMap(const glm::vec2& pos, const glm::vec2& size) const {
    glm::vec2 clamped = pos;
    clamped.x = std::clamp(clamped.x, 0.0f, size.x);
    clamped.y = std::clamp(clamped.y, 0.0f, size.y);
    return clamped;
}

} // namespace game::system
