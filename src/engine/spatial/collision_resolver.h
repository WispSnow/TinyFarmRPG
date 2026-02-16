#pragma once
#include "spatial_index_manager.h"
#include <entt/entity/registry.hpp>
#include <glm/vec2.hpp>

namespace engine::spatial {

/**
 * @brief 碰撞解析器，提供碰撞检测和移动解析功能
 * 
 * 这是一个工具类，提供碰撞查询服务，可以被多个系统复用。
 * 它不主动更新实体，而是提供查询接口供其他系统调用。
 */
class CollisionResolver {
private:
    entt::registry& registry_;
    SpatialIndexManager& spatial_index_;

public:
    CollisionResolver(entt::registry& registry, SpatialIndexManager& spatial_index);
    
    /**
     * @brief 处理实体移动，考虑碰撞检测
     * @param entity 实体ID
     * @param current_pos 当前位置
     * @param target_pos 目标位置
     * @return 实际可以移动到的位置（可能因为碰撞而小于目标位置）
     */
    [[nodiscard]] glm::vec2 resolveMovement(entt::entity entity, glm::vec2 current_pos, glm::vec2 target_pos) const;

    /**
     * @brief 检查实体是否可以移动到目标位置
     * @param entity 实体ID
     * @param target_pos 目标位置
     * @return true 可以移动，false 有碰撞
     */
    [[nodiscard]] bool canMoveTo(entt::entity entity, glm::vec2 target_pos) const;

    /**
     * @brief 同步实体在动态空间索引中的碰撞器形状（使用当前 Transform + Collider 计算）
     * @note MovementSystem 可在移动前/后调用，以减少“本帧移动阶段使用上一帧 dynamic grid 快照”的漏检概率。
     */
    void syncDynamicCollider(entt::entity entity) const;

};

} // namespace engine::spatial
