#include "ui_interactive.h"
#include "engine/core/context.h"
#include "engine/render/renderer.h"
#include "engine/resource/resource_manager.h"
#include "engine/audio/audio_player.h"
#include "engine/input/input_manager.h"
#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>
#include <entt/core/hashed_string.hpp>

using namespace entt::literals;

namespace engine::ui {

namespace {

[[nodiscard]] entt::id_type getDefaultSoundForEvent(entt::id_type event_id) {
    if (event_id == UI_SOUND_EVENT_HOVER_ID) {
        return "ui_hover"_hs;
    }
    if (event_id == UI_SOUND_EVENT_CLICK_ID) {
        return "ui_click"_hs;
    }
    return entt::null;
}

[[nodiscard]] const char* toString(InteractionPhase phase) {
    switch (phase) {
        case InteractionPhase::Normal:
            return "Normal";
        case InteractionPhase::Hovered:
            return "Hovered";
        case InteractionPhase::Pressed:
            return "Pressed";
        case InteractionPhase::Disabled:
            return "Disabled";
        default:
            return "Unknown";
    }
}

} // namespace

UIInteractive::~UIInteractive() = default;

UIInteractive::UIInteractive(engine::core::Context &context, glm::vec2 position, glm::vec2 size)
    : UIElement(std::move(position), std::move(size)), context_(context)
{
    spdlog::trace("UIInteractive 构造完成");
    last_mouse_pos_ = context_.getInputManager().getLogicalMousePosition();
}

void UIInteractive::transitionTo(InteractionPhase target_phase)
{
    const InteractionPhase source_phase = interaction_phase_;
    if (source_phase == target_phase) {
        return;
    }

    if (target_phase == InteractionPhase::Disabled) {
        setEnabled(false);
        return;
    }

    if (!interactive_) {
        return;
    }

    // 与旧实现保持一致：进入 Hover 的音效在迁移请求时触发。
    if (source_phase == InteractionPhase::Normal && target_phase == InteractionPhase::Hovered) {
        playSoundEvent(UI_SOUND_EVENT_HOVER_ID);
    }
    spdlog::trace("UIInteractive transition requested: {} -> {}",
                  toString(source_phase),
                  toString(target_phase));
    interaction_phase_ = target_phase;
    applyPhaseEnterEffects(interaction_phase_);
    notifyPhaseChanged(source_phase, interaction_phase_, "transitionTo");
}

void UIInteractive::applyPhaseEnterEffects(InteractionPhase phase)
{
    switch (phase) {
        case InteractionPhase::Normal:
            applyStateVisual(UI_IMAGE_NORMAL_ID);
            spdlog::trace("切换到正常状态");
            break;
        case InteractionPhase::Hovered:
            applyStateVisual(UI_IMAGE_HOVER_ID);
            spdlog::trace("切换到悬停状态");
            break;
        case InteractionPhase::Pressed:
            applyStateVisual(UI_IMAGE_PRESSED_ID);
            playSoundEvent(UI_SOUND_EVENT_CLICK_ID);
            spdlog::trace("切换到按下状态");
            break;
        case InteractionPhase::Disabled:
            applyStateVisual(UI_IMAGE_DISABLED_ID);
            spdlog::trace("切换到禁用状态");
            break;
        default:
            break;
    }
}

InteractionPhase UIInteractive::computeInteractionPhase() const
{
    if (!interactive_) {
        return InteractionPhase::Disabled;
    }
    if (interaction_phase_ == InteractionPhase::Disabled) {
        return InteractionPhase::Normal;
    }
    return interaction_phase_;
}

void UIInteractive::refreshInteractionPhase(std::string_view reason)
{
    const InteractionPhase old_phase = interaction_phase_;
    interaction_phase_ = computeInteractionPhase();
    notifyPhaseChanged(old_phase, interaction_phase_, reason);
}

void UIInteractive::addImage(entt::id_type name_id, engine::render::Image image)
{
    // 可交互UI元素必须有一个size用于交互检测，因此如果参数列表中没有指定，则用图片大小作为size
    if (size_.x == 0.0f && size_.y == 0.0f) {
        auto texture_size = context_.getResourceManager().getTextureSize(image.getTextureId());
        setSizeInternal(texture_size);
    }
    // 添加图片 (如果name_id已存在，则替换)
    images_.insert_or_assign(name_id, std::move(image));
}

void UIInteractive::setCurrentImage(entt::id_type name_id)
{
    if (!images_.contains(name_id)) {
        spdlog::warn("Image '{}' 未找到", name_id);
        return;
    }
    current_image_id_ = name_id;
}

void UIInteractive::applyStateVisual(entt::id_type state_id)
{
    if (images_.contains(state_id)) {
        setCurrentImage(state_id);
        return;
    }

    if (state_id != UI_IMAGE_NORMAL_ID && images_.contains(UI_IMAGE_NORMAL_ID)) {
        setCurrentImage(UI_IMAGE_NORMAL_ID);
    }
}

void UIInteractive::setInteractive(bool interactive)
{
    interactive_ = interactive;
    refreshInteractionPhase("setInteractive");
}

void UIInteractive::setEnabled(bool enabled)
{
    if (!enabled) {
        if (!interactive_) {
            applyStateVisual(UI_IMAGE_DISABLED_ID);
            return;
        }

        // 若处于按下链路，先走 release(false) 收敛回调，再进入禁用态。
        // 必须在 interactive_ = false 之前调用，否则 mouseReleased 会被 early return 短路。
        if (is_pressed_) {
            mouseReleased(false);
        }

        const InteractionPhase old_phase = interaction_phase_;
        interactive_ = false;
        is_pressed_ = false;
        is_dragging_ = false;
        interaction_phase_ = InteractionPhase::Disabled;
        applyStateVisual(UI_IMAGE_DISABLED_ID);
        notifyPhaseChanged(old_phase, interaction_phase_, "setEnabled(false)");
        return;
    }

    if (interactive_) {
        return;
    }

    const InteractionPhase old_phase = interaction_phase_;
    interactive_ = true;
    is_pressed_ = false;
    is_dragging_ = false;
    interaction_phase_ = InteractionPhase::Normal;
    applyStateVisual(UI_IMAGE_NORMAL_ID);
    notifyPhaseChanged(old_phase, interaction_phase_, "setEnabled(true)");
}

void UIInteractive::setSoundEvent(entt::id_type event_id, entt::id_type sound_id)
{
    if (event_id == entt::null) {
        return;
    }
    sound_overrides_.insert_or_assign(event_id, sound_id);
}

void UIInteractive::disableSoundEvent(entt::id_type event_id)
{
    if (event_id == entt::null) {
        return;
    }
    sound_overrides_.insert_or_assign(event_id, entt::null);
}

void UIInteractive::clearSoundEventOverride(entt::id_type event_id)
{
    sound_overrides_.erase(event_id);
}

void UIInteractive::clearSoundOverrides()
{
    sound_overrides_.clear();
}

void UIInteractive::playSoundEvent(entt::id_type event_id)
{
    if (event_id == entt::null) {
        return;
    }

    // 先尝试自定义 event->sound 覆盖
    if (auto it = sound_overrides_.find(event_id); it != sound_overrides_.end()) {
        if (it->second == entt::null) {
            return; // disabled
        }
        if (!context_.getAudioPlayer().playSound(it->second)) {
            spdlog::warn("Sound '{}' 未找到或无法播放", it->second);
        }
        return;
    }

    // 再使用默认映射（需要在 resource_mapping.json 中配置对应的 sound key）
    const entt::id_type default_sound = getDefaultSoundForEvent(event_id);
    if (default_sound == entt::null) {
        return;
    }

    if (!context_.getAudioPlayer().playSound(default_sound)) {
        spdlog::trace("Sound '{}' 未找到或无法播放", default_sound);
    }
}

void UIInteractive::update(float delta_time, engine::core::Context &context)
{
    // 先更新子节点
    UIElement::update(delta_time, context);

    if (interactive_ && is_pressed_) {
        const glm::vec2 current = context_.getInputManager().getLogicalMousePosition();
        const glm::vec2 delta = current - last_mouse_pos_;
        if (glm::length(delta) > 0.0f) {
            is_dragging_ = true;
            for (auto& behavior : behaviors_) {
                if (behavior) {
                    behavior->onDragUpdate(*this, current, delta);
                }
            }
            last_mouse_pos_ = current;
        }
    }
}

void UIInteractive::renderSelf(engine::core::Context &context)
{
    if (current_image_id_ == entt::null) {
        return;
    }

    auto it = images_.find(current_image_id_);
    if (it == images_.end()) {
        return;
    }

    const auto size = getLayoutSize();
    if (size.x <= 0.0f || size.y <= 0.0f) {
        spdlog::warn("UIInteractive 尺寸无效 ({}, {})，跳过渲染。", size.x, size.y);
        return;
    }

    context.getRenderer().drawUIImage(it->second, getScreenPosition(), size);
}

void UIInteractive::notifyPhaseChanged(InteractionPhase old_phase,
                                       InteractionPhase new_phase,
                                       std::string_view reason)
{
    if (old_phase == new_phase) {
        return;
    }

    spdlog::trace("UIInteractive phase changed: {} -> {} ({})",
                  toString(old_phase),
                  toString(new_phase),
                  reason);
    for (auto& behavior : behaviors_) {
        if (behavior) {
            behavior->onStateChanged(*this, old_phase, new_phase);
        }
    }
}

void UIInteractive::mouseEnter()
{
    if (!interactive_) return;
    if (computeInteractionPhase() == InteractionPhase::Normal) {
        transitionTo(InteractionPhase::Hovered);
    }
    for (auto& behavior : behaviors_) {
        if (behavior) {
            behavior->onHoverEnter(*this);
        }
    }
}

void UIInteractive::mouseExit()
{
    if (!interactive_) return;
    if (computeInteractionPhase() == InteractionPhase::Hovered) {
        transitionTo(InteractionPhase::Normal);
    }
    for (auto& behavior : behaviors_) {
        if (behavior) {
            behavior->onHoverExit(*this);
        }
    }
}

void UIInteractive::mousePressed()
{
    if (!interactive_) return;
    const InteractionPhase phase_before_press = computeInteractionPhase();
    is_pressed_ = true;
    last_mouse_pos_ = context_.getInputManager().getLogicalMousePosition();
    if (phase_before_press == InteractionPhase::Normal || phase_before_press == InteractionPhase::Hovered) {
        transitionTo(InteractionPhase::Pressed);
    }
    for (auto& behavior : behaviors_) {
        if (behavior) {
            behavior->onPressed(*this);
            behavior->onDragBegin(*this, last_mouse_pos_);
        }
    }
}

void UIInteractive::mouseReleased(bool is_inside)
{
    if (!interactive_) return;
    const InteractionPhase phase_before_release = computeInteractionPhase();
    const glm::vec2 current = context_.getInputManager().getLogicalMousePosition();
    for (auto& behavior : behaviors_) {
        if (behavior) {
            behavior->onDragEnd(*this, current, is_inside);
        }
    }
    is_dragging_ = false;
    is_pressed_ = false;
    if (phase_before_release == InteractionPhase::Pressed) {
        if (is_inside) {
            transitionTo(InteractionPhase::Hovered);
        } else {
            transitionTo(InteractionPhase::Normal);
        }
    }
    for (auto& behavior : behaviors_) {
        if (behavior) {
            behavior->onReleased(*this, is_inside);
        }
    }
    if (is_inside && phase_before_release == InteractionPhase::Pressed) {
        for (auto& behavior : behaviors_) {
            if (behavior) {
                behavior->onClick(*this);
            }
        }
    }
}

InteractionBehavior* UIInteractive::addBehavior(std::unique_ptr<InteractionBehavior> behavior)
{
    if (!behavior) {
        return nullptr;
    }
    behavior->onAttach(*this);
    behaviors_.push_back(std::move(behavior));
    return behaviors_.back().get();
}

glm::vec2 UIInteractive::screenToLocal(const glm::vec2& screen_pos) const {
    if (parent_) {
        const auto parent_content = parent_->getContentBounds();
        return screen_pos - parent_content.pos;
    }
    return screen_pos;
}

void UIInteractive::setPositionByScreen(const glm::vec2& screen_pos) {
    setPosition(screenToLocal(screen_pos));
}

} // namespace engine::ui
