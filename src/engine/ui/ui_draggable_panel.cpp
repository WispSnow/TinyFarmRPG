#include "ui_draggable_panel.h"
#include "engine/core/context.h"

namespace engine::ui {

UIDraggablePanel::UIDraggablePanel(engine::core::Context& context,
                                   glm::vec2 position,
                                   glm::vec2 size,
                                   std::optional<engine::utils::FColor> background_color,
                                   std::optional<engine::render::Image> skin_image,
                                   std::optional<engine::render::NineSliceMargins> skin_margins)
    : UIInteractive(context, std::move(position), std::move(size)) {
    auto panel = std::make_unique<UIPanel>(glm::vec2{0.0f, 0.0f},
                                           size,
                                           std::move(background_color),
                                           std::move(skin_image),
                                           std::move(skin_margins));
    panel->setAnchor({0, 0}, {1, 1});
    panel->setPivot({0, 0});
    content_panel_ptr_ = panel.get();
    addChild(std::move(panel));

    drag_behavior_ = std::make_unique<DragBehavior>();
    drag_behavior_->setOnBegin([this](UIInteractive&, const glm::vec2& pos) {
        drag_offset_ = pos - getScreenPosition();
    });
    drag_behavior_->setOnUpdate([this](UIInteractive&, const glm::vec2& pos, const glm::vec2&) {
        const glm::vec2 target_screen = pos - drag_offset_;
        setTopLeftByScreen(target_screen);
    });
    addBehavior(std::move(drag_behavior_));
}

void UIDraggablePanel::setTopLeftByScreen(const glm::vec2& screen_pos) {
    ensureLayout(); // 保证 layout_size_ 可用
    if (!parent_) {
        setPositionByScreen(screen_pos);
        return;
    }

    const auto parent_content = parent_->getContentBounds();
    const glm::vec2 parent_size = parent_content.size;

    // 将目标左上角转换为父内容局部坐标
    glm::vec2 top_left_local = screen_pos - parent_content.pos;

    // 反推 position_: top_left = anchor_min_pos + position_ + margin - layout_size * pivot
    glm::vec2 anchor_min_pos_local = parent_size * anchor_min_;
    glm::vec2 margin_offset{margin_.left, margin_.top};
    glm::vec2 desired_position = top_left_local - anchor_min_pos_local - margin_offset + getLayoutSize() * pivot_;

    setPosition(desired_position);
}

} // namespace engine::ui
