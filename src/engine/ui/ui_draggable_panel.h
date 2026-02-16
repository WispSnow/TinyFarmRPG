#pragma once
#include "ui_interactive.h"
#include "ui_panel.h"
#include "behavior/drag_behavior.h"
#include <memory>
#include <optional>

namespace engine::ui {

/**
 * @brief 可拖拽的面板外壳，内部包含一个 UIPanel 作为内容区域。
 */
class UIDraggablePanel final : public UIInteractive {
private:
    std::unique_ptr<UIPanel> content_panel_;
    UIPanel* content_panel_ptr_{nullptr};
    std::unique_ptr<DragBehavior> drag_behavior_;
    glm::vec2 drag_offset_{0.0f, 0.0f};

public:
    UIDraggablePanel(engine::core::Context& context,
                     glm::vec2 position,
                     glm::vec2 size,
                     std::optional<engine::utils::FColor> background_color = std::nullopt,
                     std::optional<engine::render::Image> skin_image = std::nullopt,
                     std::optional<engine::render::NineSliceMargins> skin_margins = std::nullopt);

    UIPanel* getContentPanel() const { return content_panel_ptr_; }

    // 便捷转发
    void setSkinImage(engine::render::Image image) { if (content_panel_ptr_) content_panel_ptr_->setSkinImage(std::move(image)); }
    void setNineSliceMargins(std::optional<engine::render::NineSliceMargins> margins) { if (content_panel_ptr_) content_panel_ptr_->setNineSliceMargins(std::move(margins)); }
    void setPadding(const Thickness& padding) { if (content_panel_ptr_) content_panel_ptr_->setPadding(padding); }
    void setBackgroundColor(std::optional<engine::utils::FColor> color) { if (content_panel_ptr_) content_panel_ptr_->setBackgroundColor(std::move(color)); }
    void clearSkinImage() { if (content_panel_ptr_) content_panel_ptr_->clearSkinImage(); }
    bool hasNineSliceSkin() const { return content_panel_ptr_ && content_panel_ptr_->hasNineSliceSkin(); }
    const engine::render::NineSliceMargins* getNineSliceMargins() const { return content_panel_ptr_ ? content_panel_ptr_->getNineSliceMargins() : nullptr; }
    void markNineSliceDirty() { if (content_panel_ptr_) content_panel_ptr_->markNineSliceDirty(); }

private:
    void setTopLeftByScreen(const glm::vec2& screen_pos);   ///<@brief 考虑了anchor和pivot的影响，将面板的左上角设置为屏幕上的指定位置
};

} // namespace engine::ui
