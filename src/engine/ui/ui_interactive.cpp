#include "ui_interactive.h"
#include "state/ui_state.h"
#include "state/ui_normal_state.h"
#include "state/ui_hover_state.h"
#include "state/ui_pressed_state.h"
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

[[nodiscard]] std::unique_ptr<engine::ui::state::UIState> makeLegacyStateForPhase(engine::ui::UIInteractive* owner,
                                                                                    InteractionPhase phase) {
    switch (phase) {
        case InteractionPhase::Normal:
            return std::make_unique<engine::ui::state::UINormalState>(owner);
        case InteractionPhase::Hovered:
            return std::make_unique<engine::ui::state::UIHoverState>(owner);
        case InteractionPhase::Pressed:
            return std::make_unique<engine::ui::state::UIPressedState>(owner);
        case InteractionPhase::Disabled:
            break;
        default:
            break;
    }
    return nullptr;
}

} // namespace

UIInteractive::~UIInteractive() = default;

UIInteractive::UIInteractive(engine::core::Context &context, glm::vec2 position, glm::vec2 size)
    : UIElement(std::move(position), std::move(size)), context_(context)
{
    spdlog::trace("UIInteractive 构造完成");
    last_mouse_pos_ = context_.getInputManager().getLogicalMousePosition();
}

void UIInteractive::setState(std::unique_ptr<engine::ui::state::UIState> state)
{
    if (!state) {
        spdlog::warn("尝试设置空的状态！");
        return;
    }

    state_ = std::move(state);
    refreshInteractionPhase("setState");
    applyPhaseEnterEffects(interaction_phase_);
    state_->enter();
}

void UIInteractive::setNextState(std::unique_ptr<engine::ui::state::UIState> state)
{
    next_state_ = std::move(state);
}

void UIInteractive::transitionTo(InteractionPhase target_phase)
{
    const InteractionPhase source_phase = computeInteractionPhase();
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

    // 与旧实现保持一致：进入 Hover 的音效在迁移请求时触发，不等待下帧。
    if (source_phase == InteractionPhase::Normal && target_phase == InteractionPhase::Hovered) {
        playSoundEvent(UI_SOUND_EVENT_HOVER_ID);
    }
    // 与旧实现保持一致：离开 Hover 时回调在迁移请求时触发。
    if (source_phase == InteractionPhase::Hovered && target_phase == InteractionPhase::Normal) {
        hover_leave();
    }

    auto next = makeLegacyStateForPhase(this, target_phase);
    if (!next) {
        spdlog::warn("UIInteractive transition requested unsupported phase: {}", static_cast<int>(target_phase));
        return;
    }

    spdlog::trace("UIInteractive transition requested: {} -> {}",
                  toString(source_phase),
                  toString(target_phase));
    setNextState(std::move(next));
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
            hover_enter();
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
    if (!state_) {
        return InteractionPhase::Normal;
    }
    if (state_->isPressed()) {
        return InteractionPhase::Pressed;
    }
    if (state_->isHovered()) {
        return InteractionPhase::Hovered;
    }
    return InteractionPhase::Normal;
}

void UIInteractive::refreshInteractionPhase(std::string_view reason)
{
    const InteractionPhase old_phase = interaction_phase_;
    interaction_phase_ = computeInteractionPhase();
    if (old_phase != interaction_phase_) {
        spdlog::trace("UIInteractive phase changed: {} -> {} ({})",
                      toString(old_phase),
                      toString(interaction_phase_),
                      reason);
    }
}

void UIInteractive::addImage(entt::id_type name_id, engine::render::Image image)
{
    // 可交互UI元素必须有一个size用于交互检测，因此如果参数列表中没有指定，则用图片大小作为size
    if (size_.x == 0.0f && size_.y == 0.0f) {
        auto texture_size = context_.getResourceManager().getTextureSize(image.getTextureId(), image.getTexturePath());
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
            refreshInteractionPhase("setEnabled(false)");
            return;
        }

        // 若处于按下链路，先走 release(false) 收敛回调，再进入禁用态。
        if (interactive_ && is_pressed_) {
            mouseReleased(false);
        }

        interactive_ = false;
        is_pressed_ = false;
        is_dragging_ = false;
        next_state_.reset();

        if (state_) {
            setState(std::make_unique<engine::ui::state::UINormalState>(this));
        }
        applyStateVisual(UI_IMAGE_DISABLED_ID);
        refreshInteractionPhase("setEnabled(false)");
        return;
    }

    if (interactive_) {
        return;
    }

    interactive_ = true;
    is_pressed_ = false;
    is_dragging_ = false;
    next_state_.reset();

    if (state_) {
        setState(std::make_unique<engine::ui::state::UINormalState>(this));
    }
    applyStateVisual(UI_IMAGE_NORMAL_ID);
    refreshInteractionPhase("setEnabled(true)");
}

void UIInteractive::setSoundEvent(entt::id_type event_id, entt::id_type sound_id, std::string_view path)
{
    if (event_id == entt::null) {
        return;
    }

    if (!path.empty() && sound_id != entt::null) {
        context_.getResourceManager().loadSound(sound_id, path);    // 确保音效资源被加载
    }
    sound_overrides_.insert_or_assign(event_id, sound_id);
}

void UIInteractive::setSoundEvent(entt::id_type event_id, std::string_view path)
{
    if (path.empty()) {
        disableSoundEvent(event_id);
        return;
    }

    const entt::id_type sound_id = entt::hashed_string{path.data(), path.size()}.value();
    setSoundEvent(event_id, sound_id, path);
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

    // 再更新自己（状态）
    if (state_ && interactive_) {
        if (next_state_) {
            setState(std::move(next_state_));
        }
        state_->update(delta_time, context);
    }

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

void UIInteractive::mouseEnter()
{
    if (!interactive_) return;
    // 委托给状态处理
    if (state_) state_->onMouseEnter();
    for (auto& behavior : behaviors_) {
        if (behavior) {
            behavior->onHoverEnter(*this);
        }
    }
}

void UIInteractive::mouseExit()
{
    if (!interactive_) return;
    if (state_) state_->onMouseExit();
    for (auto& behavior : behaviors_) {
        if (behavior) {
            behavior->onHoverExit(*this);
        }
    }
}

void UIInteractive::mousePressed()
{
    if (!interactive_) return;
    is_pressed_ = true;
    last_mouse_pos_ = context_.getInputManager().getLogicalMousePosition();
    if (state_) state_->onMousePressed();
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
    const glm::vec2 current = context_.getInputManager().getLogicalMousePosition();
    for (auto& behavior : behaviors_) {
        if (behavior) {
            behavior->onDragEnd(*this, current, is_inside);
        }
    }
    is_dragging_ = false;
    is_pressed_ = false;
    if (state_) state_->onMouseReleased(is_inside);
    for (auto& behavior : behaviors_) {
        if (behavior) {
            behavior->onReleased(*this, is_inside);
        }
    }
    if (is_inside) {
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
