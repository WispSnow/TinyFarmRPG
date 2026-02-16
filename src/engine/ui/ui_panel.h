#pragma once
#include "ui_element.h"
#include <optional>
#include "engine/utils/math.h"
#include "engine/render/image.h"
#include "engine/render/nine_slice.h"

namespace engine::ui {

/**
 * @brief 用于分组其他UI元素的容器UI元素
 *
 * Panel通常用于布局和组织。
 * 可以选择是否绘制背景色(纯色)。
 */
class UIPanel final : public UIElement {
    std::optional<engine::utils::FColor> background_color_;                 ///< @brief 可选背景色
    std::optional<engine::render::Image> skin_image_;                       ///< @brief 可选背景图（支持九宫格）

public:
    /**
     * @brief 构造一个Panel
     *
     * @param position Panel的局部位置  
     * @param size Panel的大小
     * @param background_color 背景色
     */
    UIPanel(glm::vec2 position = {0.0f, 0.0f},
            glm::vec2 size = {0.0f, 0.0f},
            std::optional<engine::utils::FColor> background_color = std::nullopt,
            std::optional<engine::render::Image> skin_image = std::nullopt,
            std::optional<engine::render::NineSliceMargins> skin_margins = std::nullopt);

public:
    void setBackgroundColor(std::optional<engine::utils::FColor> background_color) { background_color_ = std::move(background_color); }
    [[nodiscard]] const std::optional<engine::utils::FColor>& getBackgroundColor() const { return background_color_; }

    void setSkinImage(engine::render::Image image);
    void clearSkinImage();
    [[nodiscard]] bool hasNineSliceSkin() const;
    [[nodiscard]] const engine::render::Image* getSkinImage() const;
    [[nodiscard]] const engine::render::NineSliceMargins* getNineSliceMargins() const;
    void setNineSliceMargins(std::optional<engine::render::NineSliceMargins> margins);
    void markNineSliceDirty();

protected:
    void renderSelf(engine::core::Context& context) override;
};

} // namespace engine::ui
