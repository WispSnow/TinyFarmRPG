#include <gtest/gtest.h>

#include "engine/system/movement_system.h"

#include "engine/component/collider_component.h"
#include "engine/component/tags.h"
#include "engine/component/transform_component.h"
#include "engine/component/velocity_component.h"
#include "engine/spatial/collision_resolver.h"
#include "engine/spatial/spatial_index_manager.h"

namespace engine::system {

TEST(MovementSystemTest, MovingEntitiesDoNotEndOverlappingWhenDynamicGridIsSynced) {
    entt::registry registry;

    engine::spatial::SpatialIndexManager spatial_index;
    spatial_index.initialize(registry,
                             glm::ivec2(1, 1),
                             glm::ivec2(32, 32),
                             glm::vec2(-100.0f, -100.0f),
                             glm::vec2(100.0f, 100.0f),
                             glm::vec2(8.0f, 8.0f));

    engine::spatial::CollisionResolver resolver(registry, spatial_index);
    MovementSystem movement_system(&resolver);

    constexpr float RADIUS = 2.0f;

    const auto entity_a = registry.create();
    registry.emplace<engine::component::TransformComponent>(entity_a, glm::vec2(0.0f, 0.0f));
    registry.emplace<engine::component::VelocityComponent>(entity_a, engine::component::VelocityComponent{glm::vec2(10.0f, 0.0f)});
    registry.emplace<engine::component::CircleCollider>(entity_a, engine::component::CircleCollider{RADIUS, glm::vec2(0.0f)});
    registry.emplace<engine::component::SpatialIndexTag>(entity_a);

    const auto entity_b = registry.create();
    registry.emplace<engine::component::TransformComponent>(entity_b, glm::vec2(20.0f, 0.0f));
    registry.emplace<engine::component::VelocityComponent>(entity_b, engine::component::VelocityComponent{glm::vec2(-10.0f, 0.0f)});
    registry.emplace<engine::component::CircleCollider>(entity_b, engine::component::CircleCollider{RADIUS, glm::vec2(0.0f)});
    registry.emplace<engine::component::SpatialIndexTag>(entity_b);

    spatial_index.updateColliderEntity(entity_a);
    spatial_index.updateColliderEntity(entity_b);

    movement_system.update(registry, 1.0f);

    const auto& a_transform = registry.get<engine::component::TransformComponent>(entity_a);
    const auto& b_transform = registry.get<engine::component::TransformComponent>(entity_b);

    const glm::vec2 delta = a_transform.position_ - b_transform.position_;
    const float distance_sq = delta.x * delta.x + delta.y * delta.y;
    const float radius_sum = RADIUS * 2.0f;

    EXPECT_GT(distance_sq, radius_sum * radius_sum);
}

} // namespace engine::system
