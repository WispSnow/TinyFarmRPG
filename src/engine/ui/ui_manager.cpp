#include "ui_manager.h"
#include "ui_panel.h"
#include "ui_element.h"
#include "ui_interactive.h"
#include "ui_preset_manager.h"
#include "engine/core/context.h"
#include "engine/input/input_manager.h"
#include "engine/resource/resource_manager.h"
#include "engine/render/renderer.h"
#include "ui_drag_preview.h"
#include <entt/core/hashed_string.hpp>
#include <spdlog/spdlog.h>
#include <SDL3/SDL.h>

using namespace entt::literals;

namespace engine::ui {

namespace {
int g_hidden_system_cursor_count = 0;
} // namespace

UIManager::~UIManager()
{
    unregisterMouseEvents();
    if (hid_system_cursor_) {
        hid_system_cursor_ = false;
        if (g_hidden_system_cursor_count > 0) {
            --g_hidden_system_cursor_count;
        }

        if (g_hidden_system_cursor_count == 0) {
            if (!SDL_ShowCursor()) {
                spdlog::warn("UIManager: SDL_ShowCursor 失败: {}", SDL_GetError());
            }
        }
    }
    drag_preview_ = nullptr;
}

UIManager::UIManager(engine::core::Context& context, const glm::vec2& window_size) : context_(context)
{
    // 创建一个屏幕大小的根面板，它的子元素将基于它定位。
    root_element_ = std::make_unique<UIPanel>(glm::vec2{0.0f, 0.0f}, window_size);
    registerMouseEvents();
    initCursor();
    spdlog::trace("UI管理器已初始化根面板。");
}

void UIManager::addElement(std::unique_ptr<UIElement> element) {
    if (root_element_) {
        root_element_->addChild(std::move(element));
    } else {
        spdlog::error("无法添加元素：root_element_ 为空！");
    }
}

void UIManager::clearElements() {
    if (root_element_) {
        clearMouseState();
        root_element_->removeAllChildren();
        drag_preview_ = nullptr;
        spdlog::trace("所有UI元素已从UI管理器中清除。");
    }
}

void UIManager::update(float delta_time, engine::core::Context& context) {
    // 只处理鼠标悬停（轮询鼠标位置）
    processMouseHover();

    if (root_element_ && root_element_->isVisible()) {
        // 从根元素开始向下更新
        root_element_->update(delta_time, context);
    }
}

void UIManager::render(engine::core::Context& context) {
    if (root_element_ && root_element_->isVisible()) {
        // 从根元素开始向下渲染
        root_element_->render(context);
    }
    renderCursor(context);
}

void UIManager::beginDragPreview(const engine::render::Image& image,
                                 int count,
                                 const glm::vec2& slot_size,
                                 float alpha,
                                 std::string_view font_path) {
    if (!root_element_) {
        spdlog::warn("UIManager::beginDragPreview: 根元素为空，无法创建预览。");
        return;
    }

    if (!drag_preview_) {
        auto preview = std::make_unique<UIDragPreview>(context_, font_path, DEFAULT_UI_FONT_SIZE_PX, slot_size);
        preview->setAlpha(alpha);
        drag_preview_ = preview.get();
        root_element_->addChild(std::move(preview));
    }

    drag_preview_->setFontPath(font_path);
    drag_preview_->setAlpha(alpha);
    drag_preview_->setContent(image, count, slot_size);
    drag_preview_->setVisible(true);
}

void UIManager::updateDragPreview(const glm::vec2& screen_pos) {
    if (!drag_preview_ || !root_element_) {
        return;
    }
    auto parent_content = root_element_->getContentBounds();
    const glm::vec2 local_pos = screen_pos - parent_content.pos;
    drag_preview_->setPosition(local_pos);
}

void UIManager::endDragPreview() {
    if (drag_preview_) {
        drag_preview_->setVisible(false);
    }
}

UIInteractive* UIManager::findInteractiveAt(const glm::vec2& screen_pos) const {
    if (!root_element_ || !root_element_->isVisible()) {
        return nullptr;
    }
    return root_element_->findInteractiveAt(screen_pos);
}

UIPanel* UIManager::getRootElement() const {
    return root_element_.get();
}

void UIManager::processMouseHover() {
    if (!root_element_) {
        clearMouseState();
        return;
    }

    if (!root_element_->isVisible()) {
        clearMouseState();
        return;
    }

    // 轮询鼠标位置，更新悬停状态
    UIInteractive* target = findTargetAtMouse();
    updateHovered(target);
}

UIInteractive* UIManager::findTargetAtMouse() const {
    if (!root_element_ || !root_element_->isVisible()) {
        return nullptr;
    }

    auto& input_manager = context_.getInputManager();
    auto mouse_pos = input_manager.getLogicalMousePosition();
    return root_element_->findInteractiveAt(mouse_pos);
}

bool UIManager::onMousePressed() {
    if (!root_element_ || !root_element_->isVisible()) {
        return false; // 不阻止事件继续传播
    }

    UIInteractive* target = findTargetAtMouse();
    if (!target) {
        pressed_element_ = nullptr;
        return false;
    }

    pressed_element_ = target;
    pressed_element_->mousePressed();
    return true;
}

bool UIManager::onMouseReleased() {
    if (!root_element_ || !root_element_->isVisible()) {
        return false; // 不阻止事件继续传播
    }

    UIInteractive* target = findTargetAtMouse();
    if (pressed_element_) {
        auto* pressed = pressed_element_;
        pressed_element_ = nullptr;
        const bool is_inside = (pressed == target);
        pressed->mouseReleased(is_inside);
        return true;
    }

    // 没有捕获的按下目标：不吃掉 RELEASED，让世界逻辑仍能收到“释放”语义。
    return false;
}

void UIManager::updateHovered(UIInteractive* target) {
    if (target == hovered_element_) {
        return;
    }

    // 如果之前有悬停的元素，则离开
    if (hovered_element_) {
        hovered_element_->mouseExit();
    }

    // 更新当前悬停的元素：进入
    hovered_element_ = target;
    if (hovered_element_) {
        hovered_element_->mouseEnter();
    }
}

void UIManager::clearMouseState() {
    if (hovered_element_) {
        hovered_element_->mouseExit();
        hovered_element_ = nullptr;
    }
    pressed_element_ = nullptr;
}

void UIManager::initCursor() {
    constexpr entt::hashed_string CURSOR_PRESET_ID{"cursor"};
    hid_system_cursor_ = false;

    auto& resource_manager = context_.getResourceManager();
    const auto* preset = resource_manager.getUIPresetManager().getImagePreset(CURSOR_PRESET_ID);
    if (!preset) {
        spdlog::warn("UIManager: 未找到图片预设 '{}'，将继续使用系统鼠标。", CURSOR_PRESET_ID.data());
        cursor_image_.reset();
        cursor_size_ = {0.0f, 0.0f};
        cursor_hotspot_ = {0.0f, 0.0f};
        return;
    }

    cursor_image_ = *preset;
    cursor_size_ = preset->getSourceRect().size;
    cursor_hotspot_ = {0.0f, 0.0f};

    if (cursor_size_.x <= 0.0f || cursor_size_.y <= 0.0f) {
        cursor_size_ = resource_manager.getTextureSize(preset->getTextureId(), preset->getTexturePath());
    }

    if (cursor_size_.x <= 0.0f || cursor_size_.y <= 0.0f) {
        spdlog::warn("UIManager: 鼠标指针预设 '{}' 尺寸无效，跳过自定义鼠标。", CURSOR_PRESET_ID.data());
        cursor_image_.reset();
        return;
    }

    if (SDL_HideCursor()) {
        hid_system_cursor_ = true;
        ++g_hidden_system_cursor_count;
    } else {
        spdlog::warn("UIManager: SDL_HideCursor 失败: {}", SDL_GetError());
    }
}

void UIManager::renderCursor(engine::core::Context& context) {
    if (!cursor_image_) {
        return;
    }
    if (cursor_size_.x <= 0.0f || cursor_size_.y <= 0.0f) {
        return;
    }

    const glm::vec2 mouse_pos = context.getInputManager().getLogicalMousePosition();
    const glm::vec2 draw_pos = mouse_pos - cursor_hotspot_;
    context.getRenderer().drawUIImage(*cursor_image_, draw_pos, cursor_size_);
}

void UIManager::registerMouseEvents() {
    auto& input_manager = context_.getInputManager();
    input_manager.onAction("mouse_left"_hs, engine::input::ActionState::PRESSED).connect<&UIManager::onMousePressed>(this);
    input_manager.onAction("mouse_left"_hs, engine::input::ActionState::RELEASED).connect<&UIManager::onMouseReleased>(this);
}

void UIManager::unregisterMouseEvents() {
    auto& input_manager = context_.getInputManager();
    input_manager.onAction("mouse_left"_hs, engine::input::ActionState::PRESSED)
        .disconnect<&UIManager::onMousePressed>(this);
    input_manager.onAction("mouse_left"_hs, engine::input::ActionState::RELEASED)
        .disconnect<&UIManager::onMouseReleased>(this);
}

} // namespace engine::ui 
