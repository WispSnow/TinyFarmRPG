#include "collision_resolver.h"
#include "collider_shape.h"
#include <glm/vec2.hpp>

namespace {

// 检查是否有任何动态实体（排除 self）与 AABB 碰撞器在 pos 处重叠
bool anyDynamicOverlap(const engine::spatial::SpatialIndexManager& spatial_index,
                       const entt::registry& registry,
                       entt::entity self,
                       glm::vec2 pos,
                       const engine::component::AABBCollider& collider) {
    const auto bounds = engine::spatial::boundsRectAt(pos, collider);
    for (auto candidate : spatial_index.queryColliderCandidates(bounds)) {
        if (candidate == self) continue;
        auto shape = engine::spatial::buildColliderShape(registry, candidate);
        if (!shape) continue;
        if (engine::spatial::intersectsRectQuery(bounds, *shape)) return true;
    }
    return false;
}

// 检查是否有任何动态实体（排除 self）与圆形碰撞器在 pos 处重叠
bool anyDynamicOverlap(const engine::spatial::SpatialIndexManager& spatial_index,
                       const entt::registry& registry,
                       entt::entity self,
                       glm::vec2 pos,
                       const engine::component::CircleCollider& collider) {
    const auto center = engine::spatial::circleCenterAt(pos, collider);
    for (auto candidate : spatial_index.queryColliderCandidates(center, collider.radius_)) {
        if (candidate == self) continue;
        auto shape = engine::spatial::buildColliderShape(registry, candidate);
        if (!shape) continue;
        if (engine::spatial::intersectsCircleQuery(center, collider.radius_, *shape)) return true;
    }
    return false;
}

/// 轴分解移动解析的公共实现（AABB / Circle 通用）
/// @param half_extent  碰撞器半尺寸（AABB: size*0.5, Circle: vec2(radius)）
template <typename ColliderT>
glm::vec2 resolveMovementForShape(
    glm::vec2 current_pos,
    glm::vec2 delta,
    const ColliderT& collider,
    glm::vec2 half_extent,
    const engine::spatial::SpatialIndexManager& spatial_index,
    const engine::spatial::StaticTileGrid& static_grid,
    const entt::registry& registry,
    entt::entity entity)
{
    glm::vec2 resolved_pos = current_pos;

    const auto resolveForDelta = [&](float axis_delta, bool is_x_axis) {
        if (axis_delta == 0.0f) return;

        auto start_rect = engine::spatial::boundsRectAt(resolved_pos, collider);
        auto target_rect = start_rect;
        engine::spatial::Direction direction;
        if (is_x_axis) {
            target_rect.pos.x += axis_delta;
            direction = axis_delta > 0.0f ? engine::spatial::Direction::East : engine::spatial::Direction::West;
        } else {
            target_rect.pos.y += axis_delta;
            direction = axis_delta > 0.0f ? engine::spatial::Direction::South : engine::spatial::Direction::North;
        }

        const auto sweep = spatial_index.resolveStaticSweep(start_rect, target_rect, direction);

        glm::vec2 candidate_pos = resolved_pos;
        if (is_x_axis) {
            candidate_pos.x = sweep.rect.pos.x + half_extent.x - collider.offset_.x;
        } else {
            candidate_pos.y = sweep.rect.pos.y + half_extent.y - collider.offset_.y;
        }

        if (static_grid.hasSolidInRect(engine::spatial::boundsRectAt(candidate_pos, collider))) return;
        if (anyDynamicOverlap(spatial_index, registry, entity, candidate_pos, collider)) return;

        resolved_pos = candidate_pos;
    };

    resolveForDelta(delta.x, true);
    resolveForDelta(delta.y, false);
    return resolved_pos;
}

} // namespace

namespace engine::spatial {

CollisionResolver::CollisionResolver(entt::registry& registry, SpatialIndexManager& spatial_index)
    : registry_(registry), spatial_index_(spatial_index) {
}

glm::vec2 CollisionResolver::resolveMovement(entt::entity entity, glm::vec2 current_pos, glm::vec2 target_pos) const {
    if (!registry_.valid(entity)) {
        return current_pos;
    }

    const glm::vec2 delta = target_pos - current_pos;
    const auto& static_grid = spatial_index_.getStaticGrid();

    if (const auto* c = registry_.try_get<engine::component::AABBCollider>(entity)) {
        return resolveMovementForShape(current_pos, delta, *c,
            c->size_ * 0.5f,
            spatial_index_, static_grid, registry_, entity);
    }
    if (const auto* c = registry_.try_get<engine::component::CircleCollider>(entity)) {
        return resolveMovementForShape(current_pos, delta, *c,
            glm::vec2(c->radius_),
            spatial_index_, static_grid, registry_, entity);
    }

    return target_pos;
}

void CollisionResolver::syncDynamicCollider(entt::entity entity) const {
    spatial_index_.updateColliderEntity(entity);
}

bool CollisionResolver::canMoveTo(entt::entity entity, glm::vec2 target_pos) const {
    if (!registry_.valid(entity)) {
        return false;
    }

    const auto& static_grid = spatial_index_.getStaticGrid();

    const auto check = [&](const auto& collider) -> bool {
        if (static_grid.hasSolidInRect(engine::spatial::boundsRectAt(target_pos, collider))) return false;
        return !anyDynamicOverlap(spatial_index_, registry_, entity, target_pos, collider);
    };

    if (const auto* c = registry_.try_get<engine::component::AABBCollider>(entity)) return check(*c);
    if (const auto* c = registry_.try_get<engine::component::CircleCollider>(entity)) return check(*c);

    return true;
}

} // namespace engine::spatial
