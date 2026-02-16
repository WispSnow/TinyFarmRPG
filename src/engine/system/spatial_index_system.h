#pragma once
#include <entt/entity/registry.hpp>
#include <glm/vec2.hpp>
#include <unordered_map>

namespace engine::spatial {
    class SpatialIndexManager;
}

namespace engine::system {

/**
 * @brief 空间索引系统，负责维护动态实体网格
 * @note 使用缓存优化，只更新位置发生变化的实体
 */
class SpatialIndexSystem {
    engine::spatial::SpatialIndexManager& spatial_index_;  ///< @brief 空间索引管理器引用

public:
    explicit SpatialIndexSystem(engine::spatial::SpatialIndexManager& spatial_index);
    
    /**
     * @brief 更新所有动态实体的索引（延迟更新：只更新位置发生变化的实体）
     * @note 应该在MovementSystem之后调用
     */
    void update(entt::registry& registry);
};

} // namespace engine::system
