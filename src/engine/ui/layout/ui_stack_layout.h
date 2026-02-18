#pragma once
#include "ui_layout.h"

namespace engine::ui {

enum class Orientation {
    Horizontal,
    Vertical
};

enum class Alignment {
    Start,
    Center,
    End
};

/**
 * @brief 线性布局容器
 * 
 * 将子元素按水平或垂直方向依次排列。
 * 支持主轴方向的间距(Spacing)和内容在主轴方向的对齐方式(Alignment)。
 * 注意：当前版本 Alignment 仅作用于主轴（整体 Start/Center/End 偏移），交叉轴固定为 Start（Left/Top）。
 * 主轴长度读取 child->getLayoutSize()，会反映 stretch / override 后的最终尺寸（不做二次协商）。
 * 
 * 注意：使用StackLayout时，子元素的 Position 将被 Layout 覆盖。
 */
class UIStackLayout : public UILayout {
private:
    Orientation orientation_{Orientation::Vertical};
    float spacing_{0.0f};
    Alignment alignment_{Alignment::Start};
    Alignment cross_alignment_{Alignment::Start};
    bool auto_resize_{false}; // 是否根据内容自动调整自身大小
    
public:
    UIStackLayout(glm::vec2 position = {0.0f, 0.0f}, glm::vec2 size = {0.0f, 0.0f});

    void setOrientation(Orientation orientation);
    void setSpacing(float spacing);
    void setContentAlignment(Alignment alignment);
    void setCrossAxisAlignment(Alignment alignment);
    void setAutoResize(bool auto_resize) { auto_resize_ = auto_resize; invalidateLayout(); }

protected:
    void onLayout() override;
};

} // namespace engine::ui
