#pragma once
#include <memory>
#include <glm/vec2.hpp>
#include <entt/entity/entity.hpp>
#include <entt/signal/sigh.hpp>
#include <string_view>
#include <optional>
#include "engine/render/image.h"
#include "ui_defaults.h"

namespace engine::core {
    class Context;
}
namespace engine::ui {
    class UIElement;
    class UIPanel; // UIPanel 将作为根元素
    class UIInteractive;
    class UIDragPreview;
}

namespace engine::ui {

/**
 * @brief 管理特定场景中的UI元素集合。
 *
 * 负责UI元素的生命周期管理（通过根元素）、渲染调用和输入事件分发。
 * 每个需要UI的场景（如菜单、游戏HUD）应该拥有一个UIManager实例。
 */
class UIManager final {
private:
    engine::core::Context& context_;                         ///< @brief 上下文引用
    std::unique_ptr<UIPanel> root_element_;                  ///< @brief 一个UIPanel作为根节点(UI元素)
    UIInteractive* hovered_element_{nullptr};                ///< @brief 当前悬停的交互元素
    UIInteractive* pressed_element_{nullptr};                ///< @brief 当前按下的交互元素
    UIDragPreview* drag_preview_{nullptr};                   ///< @brief 拖拽预览层（非拥有，归根节点管理）
    std::optional<engine::render::Image> cursor_image_{};    ///< @brief 自定义鼠标指针图像（可选）
    glm::vec2 cursor_size_{0.0f, 0.0f};                      ///< @brief 鼠标指针绘制尺寸（逻辑坐标）
    glm::vec2 cursor_hotspot_{0.0f, 0.0f};                   ///< @brief 鼠标指针热点偏移（像素，逻辑坐标）
    bool hid_system_cursor_{false};                          ///< @brief 是否隐藏了系统鼠标（用于多 UIManager 叠加时恢复）
    
public:
    UIManager(engine::core::Context& context, const glm::vec2& window_size);

    ~UIManager();

    void addElement(std::unique_ptr<UIElement> element);    ///< @brief 添加一个UI元素到根节点的child_容器中。
    UIPanel* getRootElement() const;                        ///< @brief 获取根UIPanel元素的指针。
    void clearElements();                                   ///< @brief 清除所有UI元素，通常用于重置UI状态。

    // --- 核心循环方法 ---            
    void update(float delta_time, engine::core::Context&);  ///< @brief 更新UI元素。
    void render(engine::core::Context&);

    // --- 拖拽预览 ---
    void beginDragPreview(const engine::render::Image& image,
                          int count,
                          const glm::vec2& slot_size,
                          float alpha = 0.6f,
                          std::string_view font_path = DEFAULT_UI_FONT_PATH);
    void updateDragPreview(const glm::vec2& screen_pos);
    void endDragPreview();
    bool hasDragPreview() const { return drag_preview_ != nullptr; }
    UIInteractive* findInteractiveAt(const glm::vec2& screen_pos) const;

    // 禁止拷贝和移动构造/赋值
    UIManager(const UIManager&) = delete;
    UIManager& operator=(const UIManager&) = delete;
    UIManager(UIManager&&) = delete;
    UIManager& operator=(UIManager&&) = delete;

private:
    void processMouseHover();                    ///< @brief 处理鼠标悬停（轮询鼠标位置）
    UIInteractive* findTargetAtMouse() const;     ///< @brief 根据当前鼠标位置查找交互目标
    void updateHovered(UIInteractive* target);
    void clearMouseState();
    void initCursor();
    void renderCursor(engine::core::Context& context);

    bool onMousePressed();                       ///< @brief 鼠标按下事件回调
    bool onMouseReleased();                      ///< @brief 鼠标释放事件回调

    void registerMouseEvents();
    void unregisterMouseEvents();

};

} // namespace engine::ui
