#pragma once
#include "engine/ui/ui_element.h"
#include "engine/ui/ui_item_slot.h"
#include "engine/ui/layout/ui_stack_layout.h"
#include "game/data/item_catalog.h"
#include <glm/vec2.hpp>
#include <optional>
#include <vector>

namespace engine::core {
    class Context;
}

namespace engine::ui {
    class UIPanel;
    class UIStackLayout;
    class UIManager;
}

namespace game::ui {

class InventoryUI;
class ItemTooltipUI;

class HotbarUI final : public engine::ui::UIElement {
    engine::core::Context& context_;
    game::data::ItemCatalog* item_catalog_{nullptr};
    engine::ui::UIPanel* panel_{nullptr};
    engine::ui::UIStackLayout* stack_layout_{nullptr};
    engine::ui::UIManager* ui_manager_{nullptr};
    std::vector<engine::ui::UIItemSlot*> slots_{};
    std::vector<int> slot_inventory_indices_{};
    entt::entity target_{entt::null};

    glm::vec2 slot_size_{32.0f, 32.0f};
    glm::vec2 slot_spacing_{4.0f, 0.0f};
    engine::ui::Thickness panel_padding_{8.0f, 8.0f, 8.0f, 8.0f};
    int active_slot_index_{-1}; // -1 表示当前没有高亮槽位
    game::ui::InventoryUI* inventory_ui_{nullptr};
    game::ui::ItemTooltipUI* tooltip_ui_{nullptr};

    bool dragging_{false};
    int dragging_slot_{-1};
    int dragging_inventory_slot_{-1};
    std::optional<engine::ui::SlotItem> dragging_item_{};

public:
    HotbarUI(engine::core::Context& context,
             game::data::ItemCatalog* catalog = nullptr,
             glm::vec2 slot_size = {32.0f, 32.0f},
             glm::vec2 slot_spacing = {4.0f, 0.0f});

    ~HotbarUI() override;

    /// @brief 设置快捷栏槽位的物品
    void setSlotItem(int slot_index, const engine::ui::SlotItem& item);

    /// @brief 清空指定槽位
    void clearSlot(int slot_index);

    /// @brief 清空所有槽位
    void clearAllSlots();

    /// @brief 设置激活的槽位（显示选中状态）；slot_index == -1 表示清空选中状态
    void setActiveSlot(int slot_index);

    /// @brief 获取当前激活的槽位索引
    int getActiveSlotIndex() const { return active_slot_index_; }

    /// @brief 绑定 InventoryUI（用于跨UI拖拽）
    void setInventoryUI(game::ui::InventoryUI* inventory_ui) { inventory_ui_ = inventory_ui; }
    void setTooltipUI(game::ui::ItemTooltipUI* tooltip_ui) { tooltip_ui_ = tooltip_ui; }
    void setUIManager(engine::ui::UIManager* manager) { ui_manager_ = manager; }
    void setTarget(entt::entity target) { target_ = target; }

    /// @brief 设置槽位映射到物品栏索引
    void setSlotInventoryIndex(int slot_index, int inventory_index);
    void resetInventoryMappings();

    /// @brief 根据 UIItemSlot 查找对应的快捷栏索引
    int findSlotIndex(const engine::ui::UIItemSlot* slot) const;

    /// @brief 显示快捷栏
    void show();

    /// @brief 隐藏快捷栏
    void hide();

    void toggle();

private:
    void buildLayout();
    void createPanel();
    void createSlots();
    glm::vec2 calculatePanelSize() const;
    void handleDragBegin(int slot_index, engine::ui::UIInteractive& owner);
    void handleDragEnd(int slot_index, engine::ui::UIInteractive& owner, const glm::vec2& pos);
    void handleDragUpdate(engine::ui::UIInteractive& owner, const glm::vec2& pos);
    bool handleDropOnHotbar(int dst_hotbar_index);
    bool handleDropOnInventory(engine::ui::UIItemSlot* item_slot);
    void handleHoverEnter(int slot_index);
    void handleHoverExit();
    bool onMouseRightPressed();
    engine::ui::UIInteractive* findTarget(engine::ui::UIInteractive& owner, const glm::vec2& pos) const;
    std::optional<engine::ui::SlotItem> getSlotItemData(int slot_index) const;
};

} // namespace game::ui
