#pragma once
#include <entt/entity/registry.hpp>

namespace engine::spatial {
    class CollisionResolver;
}

namespace engine::system {

struct MovementSystemOptions {
    bool sync_dynamic_grid_during_movement = true;
};

/**
 * @brief 移动系统
 * 
 * 负责更新实体的移动组件，并同步到变换组件。
 * 可选地支持碰撞检测（通过 CollisionResolver）。
 */
class MovementSystem {
    engine::spatial::CollisionResolver* collision_resolver_ = nullptr;  ///< @brief 碰撞解析器（可选，用于碰撞检测）
    MovementSystemOptions options_{};

public:
    /**
     * @brief 构造函数
     * @param collision_resolver 碰撞解析器（可选，用于碰撞检测）
     */
    explicit MovementSystem(engine::spatial::CollisionResolver* collision_resolver = nullptr,
                            MovementSystemOptions options = {});

    /**
     * @brief 更新所有拥有移动和变换组件的实体
     * @param registry entt注册表
     * @param delta_time 增量时间
     */
    void update(entt::registry& registry, float delta_time);
};
}
