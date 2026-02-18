#include "ui_grid_layout.h"
#include <glm/geometric.hpp>

namespace engine::ui {

UIGridLayout::UIGridLayout(glm::vec2 position, glm::vec2 size)
    : UILayout(position, size) {
}

void UIGridLayout::setColumnCount(int count) {
    if (column_count_ != count && count > 0) {
        column_count_ = count;
        invalidateLayout();
    }
}

void UIGridLayout::setSpacing(glm::vec2 spacing) {
    if (glm::distance(spacing_, spacing) > 0.001f) {
        spacing_ = spacing;
        invalidateLayout();
    }
}

void UIGridLayout::setCellSize(glm::vec2 size) {
    if (glm::distance(cell_size_, size) > 0.001f) {
        cell_size_ = size;
        invalidateLayout();
    }
}

void UIGridLayout::onLayout() {
    if (children_.empty()) return;

    glm::vec2 start_offset = {padding_.left, padding_.top};
    const bool use_fixed_cell = cell_size_.x > 0.0f && cell_size_.y > 0.0f;
    
    int current_col = 0;
    int current_row = 0;

    for (auto& child : children_) {
        if (use_fixed_cell) {
            child->setLayoutOverrideSize(cell_size_);
        } else {
            child->clearLayoutOverrideSize();
        }

        if (!child->isVisible()) continue;

        // 确定当前子元素大小（优先使用固定 Cell Size）
        glm::vec2 size = use_fixed_cell ? cell_size_ : child->getRequestedSize();

        // 计算位置
        float x = start_offset.x + current_col * (size.x + spacing_.x);
        float y = start_offset.y + current_row * (size.y + spacing_.y);

        glm::vec2 new_pos = {x, y};
        if (glm::distance(child->getPosition(), new_pos) > 0.001f) {
            child->setPosition(new_pos);
        }

        // 更新行列索引
        current_col++;
        if (current_col >= column_count_) {
            current_col = 0;
            current_row++;
        }
    }
}

} // namespace engine::ui
