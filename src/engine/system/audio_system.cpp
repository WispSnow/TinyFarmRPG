#include "audio_system.h"
#include "engine/core/context.h"
#include "engine/component/audio_component.h"
#include "engine/component/transform_component.h"
#include "engine/audio/audio_player.h"
#include "engine/render/camera.h"
#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>
#include <entt/core/hashed_string.hpp>
#include <spdlog/spdlog.h>

namespace engine::system {

AudioSystem::~AudioSystem() {
    auto& dispatcher = context_.getDispatcher();
    dispatcher.disconnect(this);
}

AudioSystem::AudioSystem(entt::registry& registry, engine::core::Context& context)
    : registry_(registry), context_(context) {
    auto& dispatcher = context_.getDispatcher();
    dispatcher.sink<engine::utils::PlaySoundEvent>().connect<&AudioSystem::onPlaySoundEvent>(this);
}

entt::id_type AudioSystem::resolveSoundId(const engine::utils::PlaySoundEvent& event) {
    if (event.entity_ == entt::null || !registry_.valid(event.entity_)) {
        return event.sound_id_;
    }

    const auto* audio_comp = registry_.try_get<engine::component::AudioComponent>(event.entity_);
    if (!audio_comp) {
        return event.sound_id_;
    }

    if (auto it = audio_comp->sounds_.find(event.sound_id_); it != audio_comp->sounds_.end()) {
        return it->second;
    }
    return event.sound_id_;
}

void AudioSystem::onPlaySoundEvent(const engine::utils::PlaySoundEvent& event) {
    auto& audio_player = context_.getAudioPlayer();

    // Guard 1：检查 sound_id 有效性
    if (event.sound_id_ == entt::null) {
        spdlog::trace("AudioSystem: 收到空 sound_id 的 PlaySoundEvent，忽略。");
        return;
    }

    // 约定：PlaySoundEvent::sound_id_ 在不同链路下可能是：
    // - 直接可播放的 sound id（例如 resource_mapping.json 里的 sound key 的 hash）
    // - "触发名 trigger_id"（例如 ActionSoundSystem 用 action string 的 hash 作为触发名）
    //   此时需要通过 AudioComponent::sounds_ 再映射一次得到真正的 sound id。
    auto resolved_sound_id = resolveSoundId(event);

    // Guard 2：如果没有传入实体，播放全局音效
    if (event.entity_ == entt::null) {
        [[maybe_unused]] const bool played = audio_player.playSound(resolved_sound_id);
        return;
    }

    // Guard 3：如果实体无效，降级为全局音效
    if (!registry_.valid(event.entity_)) {
        spdlog::warn("AudioSystem: PlaySoundEvent entity 无效(entity={})，降级为全局音效。",
                     entt::to_integral(event.entity_));
        [[maybe_unused]] const bool played = audio_player.playSound(resolved_sound_id);
        return;
    }

    // Guard 4：如果缺少 Transform，降级为全局音效
    const auto* transform = registry_.try_get<engine::component::TransformComponent>(event.entity_);
    if (!transform) {
        spdlog::warn("AudioSystem: entity 缺少 Transform(entity={})，降级为全局音效。",
                     entt::to_integral(event.entity_));
        [[maybe_unused]] const bool played = audio_player.playSound(resolved_sound_id);
        return;
    }

    // 所有条件满足，播放 2D 空间音效
    const auto& camera_pos = context_.getCamera().getPosition();
    [[maybe_unused]] const bool played = audio_player.playSound2D(resolved_sound_id, transform->position_, camera_pos);
}

} // namespace engine::system
