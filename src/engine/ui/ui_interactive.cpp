#include "ui_interactive.h"
#include "state/ui_state.h"
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
    state_->enter();
}

void UIInteractive::setNextState(std::unique_ptr<engine::ui::state::UIState> state)
{
    next_state_ = std::move(state);
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
    // 目前只需要切换图片，但未来可能有其它逻辑（例如特效、动画等）
    setCurrentImage(state_id);
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
