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
 * 如果子元素设置了对应的轴向拉伸(Anchor)，布局会尝试调整子元素大小（暂未完全实现，目前主要控制位置）。
 * 
 * 注意：使用StackLayout时，子元素的 Position 将被 Layout 覆盖。
 */
class UIStackLayout : public UILayout {
private:
    Orientation orientation_{Orientation::Vertical};
    float spacing_{0.0f};
    Alignment alignment_{Alignment::Start};
    bool auto_resize_{false}; // 是否根据内容自动调整自身大小
    
public:
    UIStackLayout(glm::vec2 position = {0.0f, 0.0f}, glm::vec2 size = {0.0f, 0.0f});

    void setOrientation(Orientation orientation);
    void setSpacing(float spacing);
    void setContentAlignment(Alignment alignment);
    void setAutoResize(bool auto_resize) { auto_resize_ = auto_resize; invalidateLayout(); }

protected:
    void onLayout() override;
};

} // namespace engine::ui
