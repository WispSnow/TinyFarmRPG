#pragma once
#include "ui_element.h"
#include "ui_image.h"
#include "ui_label.h"
#include <string_view>

namespace engine::ui {

enum class ProgressBarFillType {
    LeftToRight,
    RightToLeft,
    BottomToTop,
    TopToBottom
};

/**
 * @brief 进度条控件
 * 
 * 包含背景图、前景填充图和可选的文本标签。
 * 支持设置 0.0 ~ 1.0 的进度值。
 */
class UIProgressBar : public UIElement {
    float value_{0.0f};
    ProgressBarFillType fill_type_{ProgressBarFillType::LeftToRight};

    // 组合子元素
    UIImage* background_image_{nullptr};
    UIImage* fill_image_{nullptr};
    UILabel* label_{nullptr};

public:
    UIProgressBar(engine::core::Context& context,
                  glm::vec2 position = {0.0f, 0.0f},
                  glm::vec2 size = {0.0f, 0.0f});

    void setValue(float value);
    float getValue() const { return value_; }

    void setBackground(const engine::render::Image& image);
    void setFill(const engine::render::Image& image);
    
    // 快捷方式：仅设置颜色（通过纯色纹理或 tint 实现，目前 Image 主要是纹理引用，这里假设传入的是简单的纯色Image）
    // void setProcessColor(const glm::vec4& color); 

    void showLabel(bool show);
    void setLabelText(std::string_view text); // 手动设置文本

    void setFillType(ProgressBarFillType type) { fill_type_ = type; updateFillVisual(); }
    
protected:
    void onLayout() override;
    void updateFillVisual();

};

} // namespace engine::ui
