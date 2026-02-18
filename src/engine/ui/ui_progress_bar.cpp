#include "ui_progress_bar.h"
#include <algorithm>
#include <memory>
#include <string>
#include <glm/geometric.hpp>

#include "engine/core/context.h"

namespace engine::ui {
namespace {
constexpr float kAnchorEpsilon = 0.001F;
}

UIProgressBar::UIProgressBar(engine::core::Context& context, glm::vec2 position, glm::vec2 size)
    : UIElement(position, size) {
    
    // 创建背景 (Order 0)
    auto bg = std::make_unique<UIImage>(engine::render::Image{}); // 空图片
    bg->setOrderIndex(0);
    bg->setAnchor({0, 0}, {1, 1}); // 充满
    background_image_ = bg.get();
    addChild(std::move(bg));

    // 创建填充 (Order 1)
    auto fill = std::make_unique<UIImage>(engine::render::Image{}); // 空图片
    fill->setOrderIndex(1);
    fill->setAnchor({0, 0}, {1, 1}); // 初始充满，通过 Layout 动态调整
    fill_image_ = fill.get();
    addChild(std::move(fill));

    // 创建标签 (Order 2) - 默认隐藏
    // 需要 TextRenderer
    auto label = std::make_unique<UILabel>(context.getTextRenderer(), "");
    label->setOrderIndex(2);
    // 这里设置为Stretch，让文本居中逻辑更简单（如果Label支持），或者Layout手动居中
    label->setAnchor({0, 0}, {1, 1}); 
    label->setVisible(false);
    label_ = label.get();
    addChild(std::move(label));

    updateFillVisual();
}

void UIProgressBar::setValue(float value) {
    value = std::clamp(value, 0.0f, 1.0f);
    if (value_ != value) {
        value_ = value;
        updateFillVisual();
    }
}

void UIProgressBar::setBackground(const engine::render::Image& image) {
    if (background_image_) {
        background_image_->setImage(image);
    }
}

void UIProgressBar::setFill(const engine::render::Image& image) {
    if (fill_image_) {
        fill_image_->setImage(image);
        updateFillVisual();
    }
}

void UIProgressBar::showLabel(bool show) {
    if (label_) {
        if (label_->isVisible() != show) {
            label_->setVisible(show);
            invalidateLayout();
        }
    }
}

void UIProgressBar::setLabelText(std::string_view text) {
    if (label_) {
        label_->setText(std::string(text));
        // UILabel usually calculates size on update.
        // We defer centering to onLayout.
        invalidateLayout(); 
    }
}

void UIProgressBar::setFillType(ProgressBarFillType type) {
    if (fill_type_ == type) {
        return;
    }
    fill_type_ = type;
    updateFillVisual();
}

void UIProgressBar::updateFillVisual() {
    if (!fill_image_) return;

    glm::vec2 anchor_min{0.0f, 0.0f};
    glm::vec2 anchor_max{1.0f, 1.0f};

    switch (fill_type_) {
        case ProgressBarFillType::LeftToRight:
            anchor_min = {0.0f, 0.0f};
            anchor_max = {value_, 1.0f};
            break;
        case ProgressBarFillType::RightToLeft:
            anchor_min = {1.0f - value_, 0.0f};
            anchor_max = {1.0f, 1.0f};
            break;
        case ProgressBarFillType::BottomToTop:
            anchor_min = {0.0f, 1.0f - value_};
            anchor_max = {1.0f, 1.0f};
            break;
        case ProgressBarFillType::TopToBottom:
            anchor_min = {0.0f, 0.0f};
            anchor_max = {1.0f, value_};
            break;
        default:
            break;
    }

    if (glm::distance(fill_image_->getAnchorMin(), anchor_min) <= kAnchorEpsilon &&
        glm::distance(fill_image_->getAnchorMax(), anchor_max) <= kAnchorEpsilon) {
        return;
    }
    fill_image_->setAnchor(anchor_min, anchor_max);
}

void UIProgressBar::onLayout() {
    // 居中标签
    if (label_ && label_->isVisible()) {
        glm::vec2 size = label_->getSize();
        // 假设 UILabel 已经计算好 Size (通过 ensureLayout)
        // Center:
        glm::vec2 my_size = getSize();
        glm::vec2 pos = (my_size - size) * 0.5f;
        // setPosition 会导致 invalidLayout -> loop? 
        // check diff
        if (glm::distance(label_->getPosition(), pos) > 0.001f) {
            label_->setPosition(pos);
        }
    }
}

} // namespace engine::ui
