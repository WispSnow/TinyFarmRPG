#include "ui_item_slot.h"
#include <algorithm>
#include <memory>
#include <string>
#include <glm/geometric.hpp> // for glm::distance
#include "engine/core/context.h" // Ensure context is fully defined if needed
#include "state/ui_normal_state.h"

namespace engine::ui {

UIItemSlot::UIItemSlot(engine::core::Context& context, glm::vec2 position, glm::vec2 size)
    : UIInteractive(context, position, size) {
    
    // 1. 图标 (Order 1) - 位于背景之上
    auto icon = std::make_unique<UIImage>(engine::render::Image{});
    icon->setOrderIndex(1);
    icon->setAnchor({0.1f, 0.1f}, {0.9f, 0.9f}); // 留一点边距
    icon->setVisible(false); // 默认隐藏，直到 setItem
    icon_image_ = icon.get();
    addChild(std::move(icon));

    // 2. 冷却 (Order 2) - 遮挡图标
    // 用纯色 Panel 作为遮罩，避免依赖额外贴图资源。
    // 这里我们不仅控制 Visible，还通过 Anchor 控制覆盖高度。
    auto cd = std::make_unique<UIPanel>(glm::vec2{0.0f, 0.0f},
                                        glm::vec2{0.0f, 0.0f},
                                        engine::utils::FColor{0.0f, 0.0f, 0.0f, 0.55f});
    cd->setOrderIndex(2);
    cd->setAnchor({0, 0}, {1, 0}); // 初始高度为0
    cd->setVisible(false);
    cooldown_overlay_ = cd.get();
    addChild(std::move(cd));

    // 3. 数量 (Order 3) - 位于右下角
    auto lbl = std::make_unique<UILabel>(context.getTextRenderer(), "");
    lbl->setOrderIndex(3);
    lbl->setAnchor({0.0f, 0.0f}, {0.0f, 0.0f}); // 使用绝对坐标，便于精确放在图标内
    lbl->setVisible(false);
    count_label_ = lbl.get();
    addChild(std::move(lbl));

    // 4. 选中框 (Order 4) - 最上层
    auto sel = std::make_unique<UIImage>(engine::render::Image{});
    sel->setOrderIndex(4);
    sel->setAnchor({0, 0}, {1, 1}); // 充满
    sel->setVisible(false);
    selection_frame_ = sel.get();
    addChild(std::move(sel));

    disableHoverSound();
    disableClickSound();

    // 初始化交互状态机，确保悬停/点击事件可用
    setState(std::make_unique<engine::ui::state::UINormalState>(this));
}

void UIItemSlot::setItem(const engine::render::Image& icon, int count) {
    setSlotItem(entt::null, count, icon);
}

void UIItemSlot::setSlotItem(entt::id_type item_id, int count, const engine::render::Image& icon) {
    if (count <= 0) {
        slot_item_.reset();
        clearItem();
        return;
    }
    slot_item_ = SlotItem{item_id, count, icon};
    setItemIcon(icon);
    setItemCount(count);
}

void UIItemSlot::setSlotItem(const SlotItem& item) {
    setSlotItem(item.item_id, item.count, item.icon);
}

void UIItemSlot::setItemIcon(const engine::render::Image& icon) {
    if (icon_image_) {
        icon_image_->setImage(icon);
        icon_image_->setVisible(true); // 显示图标
    }
    if (slot_item_) {
        slot_item_->icon = icon;
    }
}

glm::vec2 UIItemSlot::getIconLayoutSize() const {
    if (icon_image_) {
        glm::vec2 size = icon_image_->getLayoutSize();
        if (size.x > 0.0f && size.y > 0.0f) {
            return size;
        }
        return icon_image_->getRequestedSize();
    }
    return {0.0f, 0.0f};
}

void UIItemSlot::setItemCount(int count) {
    if (count_label_) {
        if (count > 1) {
            count_label_->setText(std::to_string(count));
            count_label_->setVisible(true);
        } else {
            count_label_->setVisible(false);
        }
        invalidateLayout(); // 文本长度变了可能需要对齐
    }
    if (slot_item_) {
        slot_item_->count = std::max(count, 0);
    }
}

void UIItemSlot::clearItem() {
    if (icon_image_) icon_image_->setVisible(false);
    if (count_label_) count_label_->setVisible(false);
    slot_item_.reset();
}

void UIItemSlot::setSelected(bool selected) {
    selected_ = selected;
    if (selection_frame_) {
        selection_frame_->setVisible(selected);
    }
}

void UIItemSlot::setCooldown(float percent) {
    if (cooldown_overlay_) {
        percent = std::clamp(percent, 0.0f, 1.0f);
        cooldown_percent_ = percent;
        if (percent > 0.001f) {
            cooldown_overlay_->setVisible(true);
            // 垂直遮罩: AnchorTop = percent
            // "剩余冷却时间/总时间" -> 1.0 表示刚开始冷却(全黑), 0.0 表示冷却结束(无遮挡)
            cooldown_overlay_->setAnchor({0, 0}, {1, percent}); 
        } else {
            cooldown_overlay_->setVisible(false);
        }
    }
}

void UIItemSlot::setSelectionImage(const engine::render::Image& image) {
    if (selection_frame_) {
        selection_frame_->setImage(image);
    }
}

void UIItemSlot::setBackgroundImage(const engine::render::Image& image) {
    addImage(UI_IMAGE_NORMAL_ID, image);
    setCurrentImage(UI_IMAGE_NORMAL_ID);
}

void UIItemSlot::applyStateVisual(entt::id_type state_id) {
    if (images_.contains(state_id)) {
        setCurrentImage(state_id);
        return;
    }

    if (state_id != UI_IMAGE_NORMAL_ID && images_.contains(UI_IMAGE_NORMAL_ID)) {
        setCurrentImage(UI_IMAGE_NORMAL_ID);
    }
}

void UIItemSlot::onLayout() {
    // 调整 Count Label 位置，使其靠右下对齐
    if (count_label_ && count_label_->isVisible()) {
        // 假设 Label 的 Size 已计算
        glm::vec2 lbl_size = count_label_->getSize();
        glm::vec2 padding = {2.0f, 2.0f}; // 图标内部的小边距

        // 优先放在图标内部的右下角
        glm::vec2 pos = getPosition(); // fallback 使用父坐标系
        if (icon_image_) {
            const auto icon_bounds = icon_image_->getBounds(); // 屏幕坐标
            const auto parent_screen_pos = getScreenPosition();
            glm::vec2 icon_local_pos = icon_bounds.pos - parent_screen_pos; // 转为父局部坐标

            pos.x = icon_local_pos.x + icon_bounds.size.x - lbl_size.x - padding.x;
            pos.y = icon_local_pos.y + icon_bounds.size.y - lbl_size.y - padding.y;
        } else {
            // 无图标时退化为贴紧自身右下角
            glm::vec2 my_size = getSize();
            pos.x = my_size.x - lbl_size.x - padding.x;
            pos.y = my_size.y - lbl_size.y - padding.y;
        }
        
        if (glm::distance(count_label_->getPosition(), pos) > 0.001f) {
            count_label_->setPosition(pos);
        }
    }
}

} // namespace engine::ui
