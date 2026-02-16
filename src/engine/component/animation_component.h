#pragma once
#include "engine/utils/math.h"
#include <entt/core/hashed_string.hpp>
#include <entt/entity/entity.hpp>
#include <unordered_map>
#include <vector>

namespace engine::component {

/**
 * @brief 动画帧数据结构
 * 
 * 包含帧源矩形和帧间隔（毫秒）。
 */
struct AnimationFrame {
    engine::utils::Rect src_rect_{};        ///< @brief 帧源矩形
    float duration_ms_{100.0f};             ///< @brief 帧间隔（毫秒）
    AnimationFrame(const engine::utils::Rect& src_rect, float duration_ms = 100.0f)
     : src_rect_(src_rect), duration_ms_(duration_ms) {}
};

/**
 * @brief 动画数据结构
 * 
 * 包含动画名称、帧列表、总时长、当前播放时间、是否循环等属性。
 */
struct Animation {
    std::string name_;
    entt::id_type texture_id_{entt::null};
    std::string texture_path_;
    glm::vec2 pivot_{0.0f};
    glm::vec2 dst_size_{0.0f};
    std::vector<AnimationFrame> frames_;
    std::unordered_map<int, entt::id_type> events_; ///< @brief 动画事件，键为帧索引(0-based)，值为事件ID
    bool loop_{true};
    bool flip_horizontal_{false};

    Animation() = default;
    Animation(std::string_view name, glm::vec2 dst_size, std::vector<AnimationFrame> frames) : 
        name_(name), dst_size_(dst_size), frames_(std::move(frames)) {}
    Animation(std::string_view name, glm::vec2 pivot, glm::vec2 dst_size, std::vector<AnimationFrame> frames, 
    std::unordered_map<int, entt::id_type> events, bool loop = true) :
        name_(name), pivot_(pivot), dst_size_(dst_size), frames_(std::move(frames)), events_(std::move(events)), loop_(loop) {}
};

/**
 * @brief 动画组件
 * 
 * 包含动画名称、帧列表、总时长、当前播放时间、是否循环等属性。
 */
struct AnimationComponent {
    std::unordered_map<entt::id_type, Animation> animations_;   ///< @brief 动画集合
    entt::id_type current_animation_id_{entt::null};            ///< @brief 当前播放的动画名称
    size_t current_frame_index_{};                              ///< @brief 当前播放的帧索引
    float current_time_ms_{};                                   ///< @brief 当前播放时间（毫秒）
    float speed_{1.0f};                                         ///< @brief 播放速度

    AnimationComponent() = default;

    /**
     * @brief 构造函数
     * @param animations 动画集合
     * @param current_animation_name 当前播放的动画名称
     * @param current_frame_index 当前播放的帧索引
     * @param current_time_ms 当前播放时间（毫秒）
     * @param speed 播放速度
     */
    AnimationComponent(std::unordered_map<entt::id_type, Animation> animations,
                       entt::id_type current_animation_id,
                       size_t current_frame_index = 0,
                       float current_time_ms = 0.0f,
                       float speed = 1.0f) : 
                       animations_(std::move(animations)),
                       current_animation_id_(current_animation_id),
                       current_frame_index_(current_frame_index),
                       current_time_ms_(current_time_ms),
                       speed_(speed) {}
};

} // namespace engine::component
