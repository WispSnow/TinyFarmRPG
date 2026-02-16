#include "movement_system.h"
#include "engine/spatial/collision_resolver.h"
#include "engine/component/velocity_component.h"
#include "engine/component/transform_component.h"
#include "engine/component/collider_component.h"
#include "engine/component/tags.h"
#include <limits>
#include <glm/gtc/epsilon.hpp>

namespace engine::system {

MovementSystem::MovementSystem(engine::spatial::CollisionResolver* collision_resolver, MovementSystemOptions options)
    : collision_resolver_(collision_resolver), options_(options) {
}

void MovementSystem::update(entt::registry& registry, float delta_time) {
    // 获取感兴趣的实体 view
    auto view = registry.view<engine::component::VelocityComponent, engine::component::TransformComponent>();

    // 遍历获取的实体，获取组件并执行相关逻辑
    for (auto entity : view) {
        const auto& velocity = view.get<engine::component::VelocityComponent>(entity);
        auto& transform = view.get<engine::component::TransformComponent>(entity);

        glm::vec2 current_pos = transform.position_;
        glm::vec2 target_pos = current_pos + velocity.velocity_ * delta_time;
        
        // 如果有碰撞解析器，进行碰撞检测
        if (collision_resolver_) {
            // 检查是否有碰撞器组件
            bool has_collider = registry.any_of<engine::component::AABBCollider, engine::component::CircleCollider>(entity);
            
            if (has_collider) {
                // 说明：SpatialIndexSystem 通常在移动系统之后才批量同步 dynamic grid。
                // 为减少本帧内“移动实体互相漏检”的概率，这里对当前实体做一次就地同步：
                // - 移动前：确保本实体在 dynamic grid 中的形状与当前位置一致（处理上一系统改 Transform 的情况）
                // - 移动后：让后续实体能查询到本实体的最新位置
                if (options_.sync_dynamic_grid_during_movement && registry.any_of<engine::component::SpatialIndexTag>(entity)) {
                    collision_resolver_->syncDynamicCollider(entity);
                }

                // 使用碰撞解析器处理移动（支持滑动碰撞）
                glm::vec2 resolved_pos = collision_resolver_->resolveMovement(entity, current_pos, target_pos);
                transform.position_ = resolved_pos;

                if (options_.sync_dynamic_grid_during_movement && registry.any_of<engine::component::SpatialIndexTag>(entity)) {
                    collision_resolver_->syncDynamicCollider(entity);
                }
            } else {
                // 没有碰撞器，直接移动
                transform.position_ = target_pos;
            }
        } else {
            // 没有碰撞解析器，直接移动
            transform.position_ = target_pos;
        }

        // 如果位置发生变化，则标记变换脏
        if (auto equals = glm::epsilonEqual(transform.position_, current_pos, std::numeric_limits<float>::epsilon()); !equals.x || !equals.y) {
            registry.emplace_or_replace<engine::component::TransformDirtyTag>(entity);
        }
    }
}
}   // namespace engine::system
