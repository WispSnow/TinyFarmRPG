#pragma once

#include <entt/entity/registry.hpp>

namespace engine::render {
    class Camera;
}

namespace engine::input {
    class InputManager;
}

namespace game::system {

/**
 * @brief 相机跟随系统
 * 
 * 负责让相机平滑跟随玩家实体
 */
class CameraFollowSystem {
    entt::registry& registry_;
    engine::render::Camera& camera_;
    engine::input::InputManager& input_manager_;

    float follow_speed_{5.0f}; ///< @brief 相机跟随速度（用于平滑插值）
    float stop_distance_threshold_{2.0f}; ///< @brief 距离目标小于该阈值时停止移动，避免最后一帧跳变

public:
    CameraFollowSystem(entt::registry& registry, engine::render::Camera& camera, engine::input::InputManager& input_manager);
    
    /**
     * @brief 更新相机位置，使其跟随玩家
     * @param delta_time 增量时间
     */
    void update(float delta_time);

    /**
     * @brief 鼠标滚轮缩放相机（轮询）
     */
    void updateMouseWheel(float delta_time);
    
    /**
     * @brief 设置相机跟随速度
     * @param speed 跟随速度（值越大跟随越快，建议范围 1.0f ~ 20.0f）
     */
    void setFollowSpeed(float speed) { follow_speed_ = speed; }
    
    /**
     * @brief 获取相机跟随速度
     */
    float getFollowSpeed() const { return follow_speed_; }

    void setStopDistanceThreshold(float threshold) { stop_distance_threshold_ = threshold < 0.0f ? 0.0f : threshold; }
    float getStopDistanceThreshold() const { return stop_distance_threshold_; }

};

} // namespace game::system
