#pragma once

#include "engine/ui/rmlui/rml_document_controller.h"
#include "engine/ui/ui_types.h"
#include "game/data/item_catalog.h"
#include "game/ui/slot_grid_support.h"

#include <RmlUi/Core/DataTypeRegister.h>
#include <RmlUi/Core/Types.h>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Rml {
class DataModelConstructor;
class Event;
}

namespace engine::core {
class Context;
}

namespace engine::ui::rmlui {
class RmlUiRuntime;
}

namespace game::ui {

class ItemTooltipUI;

class HotbarUI final {
    engine::ui::rmlui::RmlUiRuntime& runtime_;
    engine::core::Context& context_;
    game::data::ItemCatalog* item_catalog_{nullptr};
    uint64_t owner_scene_id_{0};

    engine::ui::rmlui::RmlDocumentController document_controller_{};
    Rml::DataTypeRegister type_register_{};

    std::vector<SlotGridViewModel> hotbar_slots_{};
    std::vector<std::optional<engine::ui::SlotItem>> slot_items_{};
    std::vector<int> slot_inventory_indices_{};
    entt::entity target_{entt::null};
    int active_slot_index_{-1};
    int hovered_slot_index_{-1};
    bool visible_{true};
    bool data_types_registered_{false};

    game::ui::ItemTooltipUI* tooltip_ui_{nullptr};

    SlotGridDragState drag_state_{};

public:
    HotbarUI(engine::ui::rmlui::RmlUiRuntime& runtime,
             engine::core::Context& context,
             uint64_t owner_scene_id,
             game::data::ItemCatalog* catalog = nullptr);
    ~HotbarUI();

    HotbarUI(const HotbarUI&) = delete;
    HotbarUI& operator=(const HotbarUI&) = delete;
    HotbarUI(HotbarUI&&) = delete;
    HotbarUI& operator=(HotbarUI&&) = delete;

    [[nodiscard]] bool isReady() const {
        return document_controller_.document() != nullptr && document_controller_.isModelValid();
    }
    [[nodiscard]] bool isVisible() const { return visible_; }

    void setSlotItem(int slot_index, const engine::ui::SlotItem& item);
    void clearSlot(int slot_index);
    void clearAllSlots();
    void setActiveSlot(int slot_index);
    [[nodiscard]] int getActiveSlotIndex() const { return active_slot_index_; }

    void setTooltipUI(game::ui::ItemTooltipUI* tooltip_ui) { tooltip_ui_ = tooltip_ui; }
    void setTarget(entt::entity target) { target_ = target; }

    void setSlotInventoryIndex(int slot_index, int inventory_index);
    void resetInventoryMappings();

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
