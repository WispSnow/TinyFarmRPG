#include "ui_stack_layout.h"
#include <cmath>
#include <glm/geometric.hpp>

namespace engine::ui {

UIStackLayout::UIStackLayout(glm::vec2 position, glm::vec2 size)
    : UILayout(position, size) {
}

void UIStackLayout::setOrientation(Orientation orientation) {
    if (orientation_ != orientation) {
        orientation_ = orientation;
        invalidateLayout();
    }
}

void UIStackLayout::setSpacing(float spacing) {
    if (std::abs(spacing_ - spacing) > 0.001f) {
        spacing_ = spacing;
        invalidateLayout();
    }
}

void UIStackLayout::setContentAlignment(Alignment alignment) {
    if (alignment_ != alignment) {
        alignment_ = alignment;
        invalidateLayout();
    }
}

void UIStackLayout::onLayout() {
    if (children_.empty()) return;

    glm::vec2 content_start = {padding_.left, padding_.top};
    glm::vec2 content_size = layout_size_ - glm::vec2(padding_.width(), padding_.height());
    
    // 确保内容区域大小不为负
    content_size = glm::max(content_size, glm::vec2(0.0f));

    float current_pos = 0.0f;
    bool is_vertical = (orientation_ == Orientation::Vertical);

    // 第一次遍历：计算总占用空间（用于居中或对齐）
    float total_content_length = 0.0f;
    for (const auto& child : children_) {
        if (!child->isVisible()) continue;
        
        // 强制确保子元素布局已更新（基于当前父元素尺寸）
        glm::vec2 child_size = child->getRequestedSize(); 
        
        float length = is_vertical ? child_size.y : child_size.x;
        total_content_length += length;
    }
    
    // 加上间距
    int visible_count = 0;
    for (const auto& child : children_) {
        if (child->isVisible()) ++visible_count;
    }
    if (visible_count > 1) {
        total_content_length += (visible_count - 1) * spacing_;
    }

    // 根据对齐方式计算起始偏移
    if (is_vertical) {
        if (alignment_ == Alignment::Center) {
            current_pos = (content_size.y - total_content_length) * 0.5f;
        } else if (alignment_ == Alignment::End) {
            current_pos = content_size.y - total_content_length;
        }
    } else {
        if (alignment_ == Alignment::Center) {
            current_pos = (content_size.x - total_content_length) * 0.5f;
        } else if (alignment_ == Alignment::End) {
            current_pos = content_size.x - total_content_length;
        }
    }

    // 起始位置包含 padding
    float start_offset_x = content_start.x;
    float start_offset_y = content_start.y;

    // 第二次遍历：设置位置
    for (auto& child : children_) {
        if (!child->isVisible()) continue;

        glm::vec2 child_pos = child->getPosition();
        glm::vec2 child_req_size = child->getRequestedSize();
        
        glm::vec2 new_pos = child_pos; // 默认保留原值，只修改受控轴

        if (is_vertical) {
            new_pos.y = start_offset_y + current_pos;
             new_pos.x = start_offset_x; // Left align

            current_pos += child_req_size.y + spacing_;
        } else {
            new_pos.x = start_offset_x + current_pos;
             new_pos.y = start_offset_y; // Top align

            current_pos += child_req_size.x + spacing_;
        }

        // 检查并更新位置
        if (glm::distance(child_pos, new_pos) > 0.001f) {
            child->setPosition(new_pos);
        }
    }

    // Auto Resize logic if needed (enlarge self to fit content)
    if (auto_resize_) {
        glm::vec2 new_size = layout_size_;
        if (is_vertical) {
            new_size.y = total_content_length + padding_.top + padding_.bottom;
        } else {
            new_size.x = total_content_length + padding_.left + padding_.right; 
        }
        
        if (glm::distance(size_, new_size) > 0.001f) {
            setSizeInternal(new_size); // Update self size
        }
    }
}

} // namespace engine::ui
