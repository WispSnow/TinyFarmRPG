#pragma once
#include "engine/utils/events.h"
#include <entt/entity/fwd.hpp>
#include <entt/signal/fwd.hpp>


namespace game::system {

/**
 * @brief 动画事件系统，用于处理各种动画事件
 */
 class AnimationEventSystem {
    entt::registry& registry_;
    entt::dispatcher& dispatcher_;

public:
    AnimationEventSystem(entt::registry& registry, entt::dispatcher& dispatcher);
    ~AnimationEventSystem();

private:
    // 事件回调函数
    void onAnimationEvent(const engine::utils::AnimationEvent& event);

    // 拆分不同的事件类型
    void handleToolHitEvent(const engine::utils::AnimationEvent& event);    ///< @brief 工具命中事件
    void handleSeedPlantEvent(const engine::utils::AnimationEvent& event);  ///< @brief 种子种植事件
    void handleHarvestEvent(const engine::utils::AnimationEvent& event);    ///< @brief 收获事件
};

} // namespace game::system