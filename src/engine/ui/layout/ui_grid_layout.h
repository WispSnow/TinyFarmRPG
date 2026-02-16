#pragma once
#include "ui_layout.h"

namespace engine::ui {

/**
 * @brief 网格布局容器
 * 
 * 将子元素按网格排列。通常用于物品栏、技能栏等。
 * 支持固定列数或固定行数（目前优先实现固定列数）。
 * 支持固定单元格大小，或使用第一个子元素的大小作为参考。
 */
class UIGridLayout : public UILayout {
    int column_count_{5};       // 默认列数
    glm::vec2 spacing_{0.0f};   // 单元格间距
    glm::vec2 cell_size_{0.0f}; // 单元格大小 (0,0 表示使用子元素自身大小，建议统一设置)
    
public:
    UIGridLayout(glm::vec2 position = {0.0f, 0.0f}, glm::vec2 size = {0.0f, 0.0f});

    void setColumnCount(int count);
    void setSpacing(glm::vec2 spacing);
    void setCellSize(glm::vec2 size);

protected:
    void onLayout() override;
};

} // namespace engine::ui
