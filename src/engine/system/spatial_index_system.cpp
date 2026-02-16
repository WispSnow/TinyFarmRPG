#include "spatial_index_system.h"
#include "engine/spatial/spatial_index_manager.h"
#include "engine/component/tags.h"
#include "engine/component/transform_component.h"
#include "engine/component/collider_component.h"
#include <spdlog/spdlog.h>
#include <cstdint>

namespace engine::system {

SpatialIndexSystem::SpatialIndexSystem(engine::spatial::SpatialIndexManager& spatial_index)
    : spatial_index_(spatial_index) {
}

void SpatialIndexSystem::update(entt::registry& registry) {

    // 延迟更新：只更新位置发生变化的实体
    auto view = registry.view<
        engine::component::SpatialIndexTag,
        engine::component::TransformDirtyTag,
        engine::component::TransformComponent
    >();
    
    for (auto entity : view) {
        // 检查是否有碰撞器组件
        const bool has_aabb = registry.all_of<engine::component::AABBCollider>(entity);
        const bool has_circle = registry.all_of<engine::component::CircleCollider>(entity);

        if (!has_aabb && !has_circle) {
            spdlog::warn("SpatialIndexSystem: entity {} has SpatialIndexTag but no collider; clearing TransformDirtyTag",
                         static_cast<std::uint32_t>(entity));
        }

        spatial_index_.updateColliderEntity(entity);
        registry.remove<engine::component::TransformDirtyTag>(entity);
    }
}

} // namespace engine::system
