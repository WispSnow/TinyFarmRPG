#pragma once

#include "engine/ui/rmlui/rml_data_bridge.h"
#include "engine/ui/ui_item_slot.h"
#include "game/data/item_catalog.h"

#include <RmlUi/Core/DataTypeRegister.h>
#include <RmlUi/Core/Types.h>

#include <optional>
#include <vector>

namespace Rml {
class DataModelConstructor;
class ElementDocument;
class Event;
}

namespace engine::core {
class Context;
}

namespace engine::ui::rmlui {
class RmlUILayer;
}

namespace game::defs {
struct InventoryChanged;
}

namespace game::ui {

class HotbarUI;
class ItemTooltipUI;

class InventoryUI final {
    struct InventorySlotViewModel {
        int local_slot_index{0};
        int inventory_index{0};
        Rml::String icon_decorator{};
        Rml::String count_text{};
        bool has_item{false};
        bool has_count{false};
        bool can_drag{false};
    };

    engine::ui::rmlui::RmlUILayer& layer_;
    engine::core::Context& context_;
    game::data::ItemCatalog* item_catalog_{nullptr};
    uint64_t owner_scene_id_{0};

    engine::ui::rmlui::RmlDataBridge data_bridge_{};
    Rml::DataTypeRegister type_register_{};
    Rml::ElementDocument* document_{nullptr};

    std::vector<InventorySlotViewModel> inventory_slots_{};
    std::vector<std::optional<engine::ui::SlotItem>> slot_items_{};
    Rml::String page_text_{};
    entt::entity target_{entt::null};
    int current_page_{0};
    int hovered_inventory_index_{-1};
    bool visible_{false};
    bool data_types_registered_{false};

    game::ui::HotbarUI* hotbar_ui_{nullptr};
    game::ui::ItemTooltipUI* tooltip_ui_{nullptr};

    bool dragging_{false};
    bool drop_handled_{false};
    int dragging_inventory_index_{-1};

public:
    InventoryUI(engine::ui::rmlui::RmlUILayer& layer,
                engine::core::Context& context,
                uint64_t owner_scene_id,
                game::data::ItemCatalog* catalog = nullptr);
    ~InventoryUI();

    InventoryUI(const InventoryUI&) = delete;
    InventoryUI& operator=(const InventoryUI&) = delete;
    InventoryUI(InventoryUI&&) = delete;
    InventoryUI& operator=(InventoryUI&&) = delete;

    [[nodiscard]] bool isReady() const { return document_ != nullptr && data_bridge_.isValid(); }
    [[nodiscard]] bool isVisible() const { return visible_; }
    [[nodiscard]] bool isDragging() const { return dragging_; }
    [[nodiscard]] int getDragInventoryIndex() const { return dragging_inventory_index_; }
    void notifyExternalDropHandled() { drop_handled_ = true; }

    void setSlotItem(int index, const engine::ui::SlotItem& item);
    void clearSlot(int index);
    void clearAllSlots();
    void setTarget(entt::entity target) { target_ = target; }
    void setHotbarUI(game::ui::HotbarUI* hotbar_ui) { hotbar_ui_ = hotbar_ui; }
    void setTooltipUI(game::ui::ItemTooltipUI* tooltip_ui) { tooltip_ui_ = tooltip_ui; }

    [[nodiscard]] int findSlotIndex(const engine::ui::UIItemSlot* slot) const {
        (void)slot;
        return -1;
    }

    [[nodiscard]] int resolveInventoryIndex(const engine::ui::UIItemSlot* slot) const {
        (void)slot;
        return -1;
    }

    void show();
    void hide();
    void toggle();

private:
    [[nodiscard]] bool initDocument();
    [[nodiscard]] bool ensureDataTypesRegistered(Rml::DataModelConstructor& constructor);
    void destroyDocument();

    [[nodiscard]] static bool isValidInventoryIndex(int inventory_index);
    [[nodiscard]] static bool isValidVisibleSlotIndex(int local_slot_index);
    void updatePageText();
    void refreshAllSlotViewModels();
    void refreshSlotViewModel(int local_slot_index);
    void markSlotsDirty();
    void markPageDirty();

    [[nodiscard]] std::optional<engine::ui::SlotItem> getSlotItemData(int inventory_index) const;
    void showTooltipForSlot(int inventory_index);
    void refreshTooltipForHoveredSlot();
    void clearTooltip();
    void clearDragState();
    void changePage(int delta);
    void onInventoryChanged(const game::defs::InventoryChanged& evt);

    void onClose(Rml::Event& event);
    void onPageLeft(Rml::Event& event);
    void onPageRight(Rml::Event& event);
    void onSlotMouseUp(int inventory_index, Rml::Event& event);
    void onSlotHoverEnter(int inventory_index, Rml::Event& event);
    void onSlotHoverExit(int inventory_index, Rml::Event& event);
    void onSlotDragStart(int inventory_index, Rml::Event& event);
    void onSlotDragDrop(int inventory_index, Rml::Event& event);
    void onSlotDragEnd(int inventory_index, Rml::Event& event);
};

} // namespace game::ui
