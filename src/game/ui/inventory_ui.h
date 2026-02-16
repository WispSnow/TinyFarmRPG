#pragma once
#include "engine/ui/ui_element.h"
#include "engine/ui/ui_item_slot.h"
#include "game/defs/events.h"
#include "game/data/item_catalog.h"
#include <glm/vec2.hpp>
#include <optional>
#include <vector>

namespace engine::core {
    class Context;
}

namespace engine::ui {
    class UIDraggablePanel;
    class UIGridLayout;
    class UILabel;
    class UIManager;
}

namespace game::ui {

class HotbarUI;
class ItemTooltipUI;

class InventoryUI final : public engine::ui::UIElement {
    engine::core::Context& context_;
    game::data::ItemCatalog* item_catalog_{nullptr};
    engine::ui::UIDraggablePanel* panel_{nullptr};
    engine::ui::UIGridLayout* grid_{nullptr};
    engine::ui::UIManager* ui_manager_{nullptr};
    engine::ui::UILabel* page_label_{nullptr};
    std::vector<engine::ui::UIItemSlot*> slots_{};
    int current_page_{0};
    entt::entity target_{entt::null};

    int columns_{5};
    int rows_{4};
    glm::vec2 slot_size_{32.0f, 32.0f};
    glm::vec2 slot_spacing_{6.0f, 6.0f};
    engine::ui::Thickness panel_padding_{12.0f, 12.0f, 12.0f, 12.0f};

    std::vector<engine::ui::SlotItem> slot_items_; // 本地缓存所有页
    game::ui::HotbarUI* hotbar_ui_{nullptr};
    game::ui::ItemTooltipUI* tooltip_ui_{nullptr};
    bool dragging_{false};
    int dragging_inventory_index_{-1};
    std::optional<engine::ui::SlotItem> dragging_item_{};

public:
    InventoryUI(engine::core::Context& context,
                game::data::ItemCatalog* catalog = nullptr,
                int columns = 5,
                int rows = 4,
                glm::vec2 slot_size = {32.0f, 32.0f},
                glm::vec2 slot_spacing = {6.0f, 6.0f});

    ~InventoryUI() override;

    void setSlotItem(int index, const engine::ui::SlotItem& item);
    void clearSlot(int index);
    void clearAllSlots();
    void setTarget(entt::entity target) { target_ = target; }
    void setHotbarUI(game::ui::HotbarUI* hotbar_ui) { hotbar_ui_ = hotbar_ui; }
    void setUIManager(engine::ui::UIManager* manager) { ui_manager_ = manager; }
    void setTooltipUI(game::ui::ItemTooltipUI* tooltip_ui) { tooltip_ui_ = tooltip_ui; }
    int findSlotIndex(const engine::ui::UIItemSlot* slot) const;
    int resolveInventoryIndex(const engine::ui::UIItemSlot* slot) const;

    void show();
    void hide();
    void toggle();

private:
    void buildLayout();
    void createPanel();
    void createCloseButton();
    void createGridAndSlots();
    void createPageButtons();
    glm::vec2 calculateGridSize() const;
    int getTotalPages() const;
    void updatePageLabel();
    void subscribeEvents();
    void onInventoryChanged(const game::defs::InventoryChanged& evt);
    void renderPage(int active_page);
    void changePage(int delta);
    void handleDragBegin(int local_index, engine::ui::UIInteractive& owner);
    void handleDragEnd(int local_index, engine::ui::UIInteractive& owner, const glm::vec2& pos);
    void handleDragUpdate(engine::ui::UIInteractive& owner, const glm::vec2& pos);
    void handleHoverEnter(int local_index);
    void handleHoverExit();
    bool onMouseRightPressed();
    std::optional<engine::ui::SlotItem> getSlotItemData(int inventory_index) const;
    engine::ui::UIInteractive* findTarget(engine::ui::UIInteractive& owner, const glm::vec2& pos) const;
};

} // namespace game::ui
