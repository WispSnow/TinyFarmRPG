#pragma once

#include "engine/component/collider_component.h"
#include "engine/component/transform_component.h"
#include "engine/utils/math.h"

#include <entt/entity/entity.hpp>
#include <entt/entity/registry.hpp>
#include <glm/common.hpp>
#include <glm/vec2.hpp>

#include <cmath>
#include <optional>

namespace engine::spatial {

enum class ColliderShapeType {
    Rect,
    Circle
};

struct ColliderShape {
    ColliderShapeType type{ColliderShapeType::Rect};
    engine::utils::Rect rect{};
    glm::vec2 center{};
    float radius{0.0f};
};

[[nodiscard]] inline engine::utils::Rect boundsRectAt(glm::vec2 position,
                                                      const engine::component::AABBCollider& collider) {
    const glm::vec2 collider_pos = position + collider.offset_;
    return engine::utils::Rect(collider_pos - collider.size_ * 0.5f, collider.size_);
}

[[nodiscard]] inline glm::vec2 circleCenterAt(glm::vec2 position, const engine::component::CircleCollider& collider) {
    return position + collider.offset_;
}

[[nodiscard]] inline engine::utils::Rect boundsRectAt(glm::vec2 position,
                                                      const engine::component::CircleCollider& collider) {
    const glm::vec2 center = circleCenterAt(position, collider);
    return engine::utils::Rect(center - glm::vec2(collider.radius_), glm::vec2(collider.radius_ * 2.0f));
}

[[nodiscard]] inline std::optional<ColliderShape> buildColliderShape(const entt::registry& registry, entt::entity entity) {
    if (!registry.valid(entity)) {
        return std::nullopt;
    }

    if (registry.all_of<engine::component::TransformComponent, engine::component::AABBCollider>(entity)) {
        const auto& transform = registry.get<engine::component::TransformComponent>(entity);
        const auto& collider = registry.get<engine::component::AABBCollider>(entity);

        ColliderShape shape;
        shape.type = ColliderShapeType::Rect;
        shape.rect = boundsRectAt(transform.position_, collider);
        return shape;
    }

    if (registry.all_of<engine::component::TransformComponent, engine::component::CircleCollider>(entity)) {
        const auto& transform = registry.get<engine::component::TransformComponent>(entity);
        const auto& collider = registry.get<engine::component::CircleCollider>(entity);

        ColliderShape shape;
        shape.type = ColliderShapeType::Circle;
        shape.center = circleCenterAt(transform.position_, collider);
        shape.radius = collider.radius_;
        return shape;
    }

    return std::nullopt;
}

[[nodiscard]] inline bool rectRectOverlap(const engine::utils::Rect& lhs, const engine::utils::Rect& rhs) {
    const glm::vec2 lhs_max = lhs.pos + lhs.size;
    const glm::vec2 rhs_max = rhs.pos + rhs.size;
    const bool separated = lhs_max.x <= rhs.pos.x || rhs_max.x <= lhs.pos.x ||
                           lhs_max.y <= rhs.pos.y || rhs_max.y <= lhs.pos.y;
    return !separated;
}

[[nodiscard]] inline bool circleCircleOverlap(const glm::vec2& lhs_center,
                                              float lhs_radius,
                                              const glm::vec2& rhs_center,
                                              float rhs_radius) {
    const glm::vec2 delta = lhs_center - rhs_center;
    const float distance_sq = delta.x * delta.x + delta.y * delta.y;
    const float radius_sum = lhs_radius + rhs_radius;
    return distance_sq <= radius_sum * radius_sum;
}

[[nodiscard]] inline bool rectCircleOverlap(const engine::utils::Rect& rect, const glm::vec2& center, float radius) {
    const glm::vec2 rect_max = rect.pos + rect.size;
    const float closest_x = glm::clamp(center.x, rect.pos.x, rect_max.x);
    const float closest_y = glm::clamp(center.y, rect.pos.y, rect_max.y);
    const float dx = center.x - closest_x;
    const float dy = center.y - closest_y;
    return (dx * dx + dy * dy) <= radius * radius;
}

[[nodiscard]] inline bool pointRectOverlap(const engine::utils::Rect& rect, const glm::vec2& point) {
    const glm::vec2 rect_max = rect.pos + rect.size;
    return point.x >= rect.pos.x && point.y >= rect.pos.y &&
           point.x < rect_max.x && point.y < rect_max.y;
}

[[nodiscard]] inline bool pointCircleOverlap(const glm::vec2& center, float radius, const glm::vec2& point) {
    const glm::vec2 delta = point - center;
    const float distance_sq = delta.x * delta.x + delta.y * delta.y;
    return distance_sq <= radius * radius;
}

[[nodiscard]] inline bool intersectsRectQuery(const engine::utils::Rect& rect, const ColliderShape& shape) {
    if (shape.type == ColliderShapeType::Rect) {
        return rectRectOverlap(rect, shape.rect);
    }
    return rectCircleOverlap(rect, shape.center, shape.radius);
}

[[nodiscard]] inline bool intersectsCircleQuery(const glm::vec2& center, float radius, const ColliderShape& shape) {
    if (shape.type == ColliderShapeType::Rect) {
        return rectCircleOverlap(shape.rect, center, radius);
    }
    return circleCircleOverlap(center, radius, shape.center, shape.radius);
}

[[nodiscard]] inline bool intersectsPointQuery(const glm::vec2& point, const ColliderShape& shape) {
    if (shape.type == ColliderShapeType::Rect) {
        return pointRectOverlap(shape.rect, point);
    }
    return pointCircleOverlap(shape.center, shape.radius, point);
}

} // namespace engine::spatial

