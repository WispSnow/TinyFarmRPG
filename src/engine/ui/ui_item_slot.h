#pragma once
#include "ui_interactive.h"
#include "ui_image.h"
#include "ui_label.h"
#include "ui_panel.h"
#include <optional>

namespace engine::ui {

struct SlotItem {
    entt::id_type item_id{entt::null};
    int count{0};
    engine::render::Image icon{};
};

/**
 * @brief 物品/技能槽位控件
 * 
 * 继承自 UIInteractive，支持点击、悬停等交互。
 * 由以下部分组成：
 * - 槽位背景（由 UIInteractive 自身状态管理）
 * - 图标 (Icon)
 * - 数量标签 (Count)
 * - 冷却遮罩 (Cooldown Overlay)
 * - 选中框 (Selection Frame)
 */
class UIItemSlot final : public UIInteractive {
private:
    UIImage* icon_image_{nullptr};
    UILabel* count_label_{nullptr};
    UIPanel* cooldown_overlay_{nullptr};
    UIImage* selection_frame_{nullptr};

    bool selected_{false};
    float cooldown_percent_{0.0f};
    std::optional<SlotItem> slot_item_{};

public:
    UIItemSlot(engine::core::Context& context,
               glm::vec2 position = {0.0f, 0.0f},
               glm::vec2 size = {0.0f, 0.0f});

    //设置物品数据
    void setItem(const engine::render::Image& icon, int count = 1);
    void setSlotItem(entt::id_type item_id, int count, const engine::render::Image& icon);
    void setSlotItem(const SlotItem& item);
    std::optional<SlotItem> getSlotItem() const { return slot_item_; }
    void clearSlotItem() { clearItem(); }
    void setItemIcon(const engine::render::Image& icon);
    glm::vec2 getIconLayoutSize() const; // 获取当前图标在布局中的实际尺寸
    void setItemCount(int count);
    void clearItem();

    // 状态控制
    void setSelected(bool selected);
    bool isSelected() const { return selected_; }
    
    void setCooldown(float percent); // 0.0 ~ 1.0 (0 = no cooldown, 1 = full cooldown)
    float getCooldown() const { return cooldown_percent_; }

    void setSelectionImage(const engine::render::Image& image);
    void setBackgroundImage(const engine::render::Image& image);

    void applyStateVisual(entt::id_type state_id) override;
    
protected:
    void onLayout() override;
};

} // namespace engine::ui
