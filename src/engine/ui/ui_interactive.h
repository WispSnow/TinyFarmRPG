#pragma once
#include "ui_element.h"
#include "state/ui_state.h"
#include "behavior/interaction_behavior.h"
#include "engine/render/image.h"   // 需要引入头文件而不是前置声明（map容器创建时可能会检查内部元素是否有析构定义）
#include <entt/core/hashed_string.hpp>
#include <unordered_map>
#include <memory>
#include <entt/entity/fwd.hpp>
#include <glm/vec2.hpp>
#include <vector>

namespace engine::core {
    class Context;
}

namespace engine::ui {

inline constexpr entt::id_type UI_IMAGE_NORMAL_ID = entt::hashed_string{"normal"}.value();
inline constexpr entt::id_type UI_IMAGE_HOVER_ID = entt::hashed_string{"hover"}.value();
inline constexpr entt::id_type UI_IMAGE_PRESSED_ID = entt::hashed_string{"pressed"}.value();
inline constexpr entt::id_type UI_IMAGE_DISABLED_ID = entt::hashed_string{"disabled"}.value();

inline constexpr entt::id_type UI_SOUND_EVENT_HOVER_ID = UI_IMAGE_HOVER_ID;
inline constexpr entt::id_type UI_SOUND_EVENT_CLICK_ID = entt::hashed_string{"click"}.value();

/**
 * @brief 可交互UI元素的基类,继承自UIElement
 *
 * 定义了可交互UI元素的通用属性和行为。
 * 管理UI状态的切换和交互逻辑。
 * 提供事件处理、更新和渲染的虚方法。
 */
class UIInteractive : public UIElement {
protected:
    engine::core::Context& context_;                        ///< @brief 可交互元素很可能需要其他引擎组件
    std::unique_ptr<engine::ui::state::UIState> state_;     ///< @brief 当前状态
    std::unique_ptr<engine::ui::state::UIState> next_state_;///< @brief 下一个状态，用于处理状态切换
    std::unordered_map<entt::id_type, engine::render::Image> images_;   ///< @brief 图片集合
    std::unordered_map<entt::id_type, entt::id_type> sound_overrides_;  ///< @brief 事件音效覆盖，key为事件ID，value为音效ID (entt::null = disabled)
    entt::id_type current_image_id_ = entt::null;           ///< @brief 当前显示的图片ID
    bool interactive_ = true;                               ///< @brief 是否可交互
    std::vector<std::unique_ptr<InteractionBehavior>> behaviors_; ///< @brief 交互行为列表
    bool is_pressed_{false};
    bool is_dragging_{false};
    glm::vec2 last_mouse_pos_{0.0f, 0.0f};

public:
    UIInteractive(engine::core::Context& context, glm::vec2 position = {0.0f, 0.0f}, glm::vec2 size = {0.0f, 0.0f});
    ~UIInteractive() override;

    virtual void clicked() {}       ///< @brief 如果有点击事件，则重写该方法
    virtual void hover_enter() {}   ///< @brief 如果有悬停进入事件，则重写该方法
    virtual void hover_leave() {}   ///< @brief 如果有悬停离开事件，则重写该方法

    void addImage(entt::id_type name_id, engine::render::Image image);      ///< @brief 添加/替换图片
    void setCurrentImage(entt::id_type name_id);                            ///< @brief 设置当前显示的图片
    virtual void applyStateVisual(entt::id_type state_id);                  ///< @brief 根据状态ID应用视觉效果

    void setSoundEvent(entt::id_type event_id, entt::id_type sound_id, std::string_view path = ""); ///< @brief 设置事件音效覆盖（可选加载路径）
    void setSoundEvent(entt::id_type event_id, std::string_view path);                                ///< @brief 通过路径设置事件音效覆盖（空路径=禁用）
    void disableSoundEvent(entt::id_type event_id);                                                   ///< @brief 禁用指定事件音效
    void clearSoundEventOverride(entt::id_type event_id);                                             ///< @brief 清除指定事件音效覆盖（恢复默认）
    void clearSoundOverrides();                                                                       ///< @brief 清除全部事件音效覆盖（恢复默认）
    void playSoundEvent(entt::id_type event_id);                                                      ///< @brief 播放事件音效

    void setHoverSound(entt::id_type id, std::string_view path = "") { setSoundEvent(UI_SOUND_EVENT_HOVER_ID, id, path); }
    void setClickSound(entt::id_type id, std::string_view path = "") { setSoundEvent(UI_SOUND_EVENT_CLICK_ID, id, path); }
    void disableHoverSound() { disableSoundEvent(UI_SOUND_EVENT_HOVER_ID); }
    void disableClickSound() { disableSoundEvent(UI_SOUND_EVENT_CLICK_ID); }
    void clearHoverSoundOverride() { clearSoundEventOverride(UI_SOUND_EVENT_HOVER_ID); }
    void clearClickSoundOverride() { clearSoundEventOverride(UI_SOUND_EVENT_CLICK_ID); }

    // --- Getters and Setters ---
    engine::core::Context& getContext() const { return context_; }
    void setState(std::unique_ptr<engine::ui::state::UIState> state);       ///< @brief 设置当前状态
    void setNextState(std::unique_ptr<engine::ui::state::UIState> state);   ///< @brief 设置下一个状态
    engine::ui::state::UIState* getState() const { return state_.get(); }   ///< @brief 获取当前状态

    void setInteractive(bool interactive) { interactive_ = interactive; }   ///< @brief 设置是否可交互
    bool isInteractive() const { return interactive_; }                     ///< @brief 获取是否可交互

    InteractionBehavior* addBehavior(std::unique_ptr<InteractionBehavior> behavior); ///< @brief 挂载行为
    void clearBehaviors() { behaviors_.clear(); }

    glm::vec2 screenToLocal(const glm::vec2& screen_pos) const;             ///< @brief 将屏幕坐标转换为本地坐标
    void setPositionByScreen(const glm::vec2& screen_pos);                 ///< @brief 通过屏幕坐标设置位置

    // 状态查询方法（委托给state_）
    bool isHovered() const { return state_ && state_->isHovered(); }        ///< @brief 查询是否处于悬停状态
    bool isPressed() const { return state_ && state_->isPressed(); }        ///< @brief 查询是否处于按下状态
    bool isDragging() const { return is_dragging_; }                        ///< @brief 查询是否处于拖拽状态

    void mouseEnter();                                                      ///< @brief 鼠标进入
    void mouseExit();                                                       ///< @brief 鼠标离开
    void mousePressed();                                                    ///< @brief 鼠标按下
    void mouseReleased(bool is_inside);                                     ///< @brief 鼠标释放，is_inside为是否在元素内部

    // --- 核心方法 ---
    void update(float delta_time, engine::core::Context& context) override;
protected:
    void renderSelf(engine::core::Context& context) override;
};

} // namespace engine::ui
