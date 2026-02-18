#include "ui_grid_layout.h"
#include <algorithm>
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
    float cursor_x = start_offset.x;
    float cursor_y = start_offset.y;
    float row_max_height = 0.0f;

    for (auto& child : children_) {
        if (use_fixed_cell) {
            child->setLayoutOverrideSize(cell_size_);
        } else {
            child->clearLayoutOverrideSize();
        }

        if (!child->isVisible()) continue;

        // 固定 cell 优先；否则使用 intrinsic requested size。最终定位采用逐项流式排布。
        glm::vec2 size = use_fixed_cell ? cell_size_ : child->getRequestedSize();
        size.x = std::max(0.0f, size.x);
        size.y = std::max(0.0f, size.y);

        glm::vec2 new_pos = {cursor_x, cursor_y};
        if (glm::distance(child->getPosition(), new_pos) > 0.001f) {
            child->setPosition(new_pos);
        }

        row_max_height = std::max(row_max_height, size.y);
        current_col++;
        if (current_col >= column_count_) {
            current_col = 0;
            cursor_x = start_offset.x;
            cursor_y += row_max_height + spacing_.y;
            row_max_height = 0.0f;
        } else {
            cursor_x += size.x + spacing_.x;
        }
    }
}

} // namespace engine::ui
