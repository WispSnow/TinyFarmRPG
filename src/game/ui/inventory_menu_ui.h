#pragma once

#include "engine/ui/rmlui/rml_data_bridge.h"
#include "engine/ui/ui_types.h"
#include "game/data/item_catalog.h"

#include <RmlUi/Core/DataTypeRegister.h>
#include <RmlUi/Core/Types.h>

#include <cstdint>
#include <entt/entity/fwd.hpp>

#include <memory>
#include <optional>
#include <vector>

namespace Rml {
class DataModelConstructor;
class Element;
class ElementDocument;
class Event;
}

namespace engine::core {
class Context;
}

namespace engine::ui::rmlui {
class HoverFocusSyncListener;
class RmlUILayer;
}

namespace game::defs {
struct InventoryChanged;
}

namespace game::ui {

class ItemTooltipUI;

class InventoryMenuUI final {
    struct InventorySlotViewModel {
        int slot_index{0};
        Rml::String icon_decorator{"none"};
        Rml::String count_text{};
        bool has_item{false};
        bool has_count{false};
        bool is_selected{false};
        bool can_drag{false};
    };

    engine::ui::rmlui::RmlUILayer& layer_;
    engine::core::Context& context_;
    game::data::ItemCatalog* item_catalog_{nullptr};
    uint64_t owner_scene_id_{0};

    engine::ui::rmlui::RmlDataBridge data_bridge_{};
    Rml::DataTypeRegister type_register_{};
    Rml::ElementDocument* document_{nullptr};
    std::unique_ptr<engine::ui::rmlui::HoverFocusSyncListener> hover_focus_listener_{};
    bool hover_listener_registered_{false};
    bool data_types_registered_{false};

    std::vector<InventorySlotViewModel> inventory_slots_{};
    std::vector<std::optional<engine::ui::SlotItem>> slot_items_{};
    std::vector<Rml::Element*> slot_buttons_{};

    entt::entity target_{entt::null};
    game::ui::ItemTooltipUI* tooltip_ui_{nullptr};

    int selected_slot_index_{-1};
    int action_menu_target_slot_{-1};
    int tooltip_slot_index_{-1};
    entt::id_type tooltip_item_id_{entt::null};

    bool action_menu_open_{false};
    bool can_use_selected_{false};
    bool can_discard_selected_{false};
    bool discard_confirm_{false};
    bool close_requested_{false};
    bool dragging_{false};
    bool drop_handled_{false};
    bool suppress_next_confirm_{false};
    int dragging_slot_index_{-1};

    Rml::String action_menu_left_{"0dp"};
    Rml::String action_menu_top_{"0dp"};
    Rml::String action_menu_title_{"Item"};
    Rml::String discard_text_{"Discard"};

public:
    InventoryMenuUI(engine::ui::rmlui::RmlUILayer& layer,
                    engine::core::Context& context,
                    uint64_t owner_scene_id,
                    game::data::ItemCatalog* catalog = nullptr);
    ~InventoryMenuUI();

    InventoryMenuUI(const InventoryMenuUI&) = delete;
    InventoryMenuUI& operator=(const InventoryMenuUI&) = delete;
    InventoryMenuUI(InventoryMenuUI&&) = delete;
    InventoryMenuUI& operator=(InventoryMenuUI&&) = delete;

    [[nodiscard]] bool isReady() const { return document_ != nullptr && data_bridge_.isValid(); }

    void setTarget(entt::entity target) { target_ = target; }
    void setTooltipUI(game::ui::ItemTooltipUI* tooltip_ui) { tooltip_ui_ = tooltip_ui; }
    void syncFromTarget();
    void queueInitialFocus();
    void update(float delta_time);

    [[nodiscard]] bool handleMenuCancel();
    [[nodiscard]] bool consumeCloseRequested();

private:
    [[nodiscard]] bool initDocument();
    [[nodiscard]] bool ensureDataTypesRegistered(Rml::DataModelConstructor& constructor);
    void destroyDocument();
    void removeEventListeners();
    void collectSlotButtons();

    [[nodiscard]] static bool isValidSlotIndex(int slot_index);
    void refreshAllSlotViewModels();
    void refreshSlotViewModel(int slot_index);
    void markSlotsDirty();
    void clearDragState();

    void refreshSelectionFromFocus();
    [[nodiscard]] int findSlotIndexForElement(Rml::Element* element) const;
    [[nodiscard]] Rml::Element* slotButton(int slot_index) const;
    void focusSlot(int slot_index);

    [[nodiscard]] std::optional<engine::ui::SlotItem> getSlotItemData(int slot_index) const;
    [[nodiscard]] const game::data::ItemData* getSlotItemMeta(int slot_index) const;
    void refreshTooltipForSelection();
    void clearTooltip();

    void openActionMenuForSlot(int slot_index);
    void closeActionMenu(bool restore_focus);
    void refreshActionMenuState();
    void updateActionMenuPlacement();

    void onInventoryChanged(const game::defs::InventoryChanged& evt);

    void onClose(Rml::Event& event);
    void onSlotConfirm(int slot_index, Rml::Event& event);
    void onSlotDragStart(int slot_index, Rml::Event& event);
    void onSlotDragDrop(int slot_index, Rml::Event& event);
    void onSlotDragEnd(int slot_index, Rml::Event& event);
    void onActionUse(Rml::Event& event);
    void onActionDiscard(Rml::Event& event);
    void onActionCancel(Rml::Event& event);
};

} // namespace game::ui
