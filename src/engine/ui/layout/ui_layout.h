#pragma once
#include "engine/ui/ui_element.h"

namespace engine::ui {

/**
 * @brief 布局容器基类
 * 
 * 为子类提供通用的布局属性（如内边距已在基类支持，这里可以扩展间距、对齐等）。
 * 需要子类实现 onLayout() 来具体安排子元素的位置和大小。
 */
class UILayout : public UIElement {
public:
    using UIElement::UIElement; // 使用基类构造函数

    ~UILayout() override = default;

    /**
     * @brief 强制更新布局
     * 
     * 通常只需要调用 ensureLayout()，但有时需要手动触发布局刷新。
     */
    void forceLayout() {
        invalidateLayout();
        ensureLayout();
    }
};

} // namespace engine::ui
