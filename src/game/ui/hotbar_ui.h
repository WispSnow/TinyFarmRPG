#pragma once

#include "engine/ui/rmlui/rml_data_bridge.h"
#include "engine/ui/ui_item_slot.h"
#include "game/data/item_catalog.h"

#include <RmlUi/Core/DataTypeRegister.h>
#include <RmlUi/Core/Types.h>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
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

namespace game::ui {

class InventoryUI;
class ItemTooltipUI;

class HotbarUI final {
    struct HotbarSlotViewModel {
        int slot_index{0};
        Rml::String icon_decorator{};
        Rml::String count_text{};
        bool has_item{false};
        bool has_count{false};
        bool is_active{false};
        bool is_bound{false};
        bool can_drag{false};
    };

    engine::ui::rmlui::RmlUILayer& layer_;
    engine::core::Context& context_;
    game::data::ItemCatalog* item_catalog_{nullptr};
    uint64_t owner_scene_id_{0};

    engine::ui::rmlui::RmlDataBridge data_bridge_{};
    Rml::DataTypeRegister type_register_{};
    Rml::ElementDocument* document_{nullptr};

    std::vector<HotbarSlotViewModel> hotbar_slots_{};
    std::vector<std::optional<engine::ui::SlotItem>> slot_items_{};
    std::vector<int> slot_inventory_indices_{};
    entt::entity target_{entt::null};
    int active_slot_index_{-1};
    int hovered_slot_index_{-1};
    bool visible_{true};
    bool data_types_registered_{false};

    game::ui::InventoryUI* inventory_ui_{nullptr};
    game::ui::ItemTooltipUI* tooltip_ui_{nullptr};

    bool dragging_{false};
    bool drop_handled_{false};
    bool suppress_next_primary_mouse_up_{false};
    int dragging_slot_{-1};
    int dragging_inventory_slot_{-1};
    std::optional<engine::ui::SlotItem> dragging_item_{};

public:
    HotbarUI(engine::ui::rmlui::RmlUILayer& layer,
             engine::core::Context& context,
             uint64_t owner_scene_id,
             game::data::ItemCatalog* catalog = nullptr);
    ~HotbarUI();

    HotbarUI(const HotbarUI&) = delete;
    HotbarUI& operator=(const HotbarUI&) = delete;
    HotbarUI(HotbarUI&&) = delete;
    HotbarUI& operator=(HotbarUI&&) = delete;

    [[nodiscard]] bool isReady() const { return document_ != nullptr && data_bridge_.isValid(); }
    [[nodiscard]] bool isVisible() const { return visible_; }

    void setSlotItem(int slot_index, const engine::ui::SlotItem& item);
    void clearSlot(int slot_index);
    void clearAllSlots();
    void setActiveSlot(int slot_index);
    [[nodiscard]] int getActiveSlotIndex() const { return active_slot_index_; }

    void setInventoryUI(game::ui::InventoryUI* inventory_ui) { inventory_ui_ = inventory_ui; }
    void setTooltipUI(game::ui::ItemTooltipUI* tooltip_ui) { tooltip_ui_ = tooltip_ui; }
    void setTarget(entt::entity target) { target_ = target; }

    void setSlotInventoryIndex(int slot_index, int inventory_index);
    void resetInventoryMappings();

    [[nodiscard]] int findSlotIndex(const engine::ui::UIItemSlot* slot) const {
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

    [[nodiscard]] bool isValidSlotIndex(int slot_index) const;
    void refreshAllSlotViewModels();
    void refreshSlotViewModel(int slot_index);
    void markSlotsDirty();

    [[nodiscard]] std::optional<engine::ui::SlotItem> getSlotItemData(int slot_index) const;
    [[nodiscard]] std::string buildIconDecorator(entt::id_type item_id) const;
    [[nodiscard]] static std::string spriteNameFromIconKey(std::string_view icon_key);

    void showTooltipForSlot(int slot_index);
    void refreshTooltipForHoveredSlot();
    void clearTooltip();
    void clearDragState();

    void onSlotMouseUp(int slot_index, Rml::Event& event);
    void onSlotHoverEnter(int slot_index, Rml::Event& event);
    void onSlotHoverExit(int slot_index, Rml::Event& event);
    void onSlotDragStart(int slot_index, Rml::Event& event);
    void onSlotDragDrop(int slot_index, Rml::Event& event);
    void onSlotDragEnd(int slot_index, Rml::Event& event);
};

} // namespace game::ui
