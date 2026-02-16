#include "animation_system.h"
#include "engine/component/animation_component.h"
#include "engine/component/sprite_component.h"
#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>
#include <spdlog/spdlog.h>

namespace engine::system {

AnimationSystem::AnimationSystem(entt::registry& registry, entt::dispatcher& dispatcher)
    : registry_(registry), dispatcher_(dispatcher) {
    dispatcher_.sink<engine::utils::PlayAnimationEvent>().connect<&AnimationSystem::onPlayAnimationEvent>(this);
}

AnimationSystem::~AnimationSystem() {
    dispatcher_.disconnect(this);
}

void AnimationSystem::update(float dt) {
    auto view = registry_.view<engine::component::AnimationComponent, engine::component::SpriteComponent>();
    for (auto entity : view) {
        auto& anim_component = view.get<engine::component::AnimationComponent>(entity);
        auto& sprite_component = view.get<engine::component::SpriteComponent>(entity);

        // 如果动画不存在，则跳过
        auto it = anim_component.animations_.find(anim_component.current_animation_id_);
        if (it == anim_component.animations_.end()) {
            continue;
        }

        // 获取当前动画
        auto& current_animation = it->second;
        // 如果没有帧，则跳过
        if (current_animation.frames_.empty()) {
            continue;
        }

        // 更新锚点、大小和翻转
        sprite_component.pivot_ = current_animation.pivot_;
        sprite_component.size_ = current_animation.dst_size_;
        sprite_component.sprite_.is_flipped_ = current_animation.flip_horizontal_;

        // 更新当前播放时间 (推进计时器)
        anim_component.current_time_ms_ += dt * 1000.0f * anim_component.speed_;

        // 获取当前帧
        const auto& current_frame = current_animation.frames_[anim_component.current_frame_index_];

        // 检查是否需要切换到下一帧
        if (anim_component.current_time_ms_ >= current_frame.duration_ms_) {
            anim_component.current_time_ms_ -= current_frame.duration_ms_;
            anim_component.current_frame_index_++;

            // 检查是否要发送动画事件
            if (current_animation.events_.find(anim_component.current_frame_index_) != current_animation.events_.end()) {
                spdlog::info("发送动画事件: {}", current_animation.events_.at(anim_component.current_frame_index_));
                dispatcher_.enqueue(engine::utils::AnimationEvent{entity, 
                    current_animation.events_.at(anim_component.current_frame_index_),
                    anim_component.current_animation_id_});
            }

            // 处理动画播放完成
            if (anim_component.current_frame_index_ >= current_animation.frames_.size()) {
                if (current_animation.loop_) {
                    anim_component.current_frame_index_ = 0;
                } else {
                    // 动画播放完毕且不循环，停在最后一帧
                    anim_component.current_frame_index_ = current_animation.frames_.size() - 1;
                    // 发送动画播放完成事件
                    dispatcher_.enqueue(engine::utils::AnimationFinishedEvent{entity, anim_component.current_animation_id_});
                }
            }
        }
        
        // 更新 SpriteComponent 的源矩形 （根据当前动画帧的源矩形信息）
        const auto& next_frame = current_animation.frames_[anim_component.current_frame_index_];
        sprite_component.sprite_.src_rect_ = next_frame.src_rect_;
    }
}

void AnimationSystem::onPlayAnimationEvent(const engine::utils::PlayAnimationEvent& event) {
    // 使用try_get方法来安全获取可能存在的组件。如果不存在则返回nullptr
    if (auto anim = registry_.try_get<engine::component::AnimationComponent>(event.entity_); anim) {
        auto it = anim->animations_.find(event.animation_id_);
        if (it == anim->animations_.end()) {
            return;
        }
        auto& animation = it->second;
        anim->current_animation_id_ = event.animation_id_;      // 替换动画ID
        anim->current_frame_index_ = 0;
        anim->current_time_ms_ = 0.0f;
        animation.loop_ = event.loop_;

        // 同步精灵纹理与基础属性
        if (auto sprite = registry_.try_get<engine::component::SpriteComponent>(event.entity_); sprite) {
            sprite->sprite_.texture_id_ = animation.texture_id_;
            sprite->sprite_.texture_path_ = animation.texture_path_;
            sprite->sprite_.is_flipped_ = animation.flip_horizontal_;
            sprite->size_ = animation.dst_size_;
            sprite->pivot_ = animation.pivot_;
        }
    }
}

} // namespace engine::system
