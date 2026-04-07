#pragma once

#include "engine/ui/rmlui/rml_document_controller.h"
#include "game/ui/menu_tab_content.h"
#include "game/ui/slot_grid_support.h"

#include <RmlUi/Core/Types.h>
#include <entt/entity/entity.hpp>

#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

namespace Rml {
class DataModelConstructor;
class Element;
class Event;
}

namespace engine::core {
class Context;
}

namespace game::data {
class ItemCatalog;
}

namespace game::defs {
struct InventoryChanged;
struct HotbarChanged;
}

namespace game::ui {

class ItemTooltipUI;

class InventoryTabContent final : public IMenuTabContent {
    struct ActionEntryViewModel {
        int action_id{0};
        Rml::String label{};
        bool is_destructive{false};
    };

    engine::core::Context& context_;
    engine::ui::rmlui::RmlDocumentController& document_controller_;
    entt::registry& game_registry_;
    entt::entity player_{entt::null};
    game::data::ItemCatalog* item_catalog_{nullptr};
    uint64_t owner_scene_id_{0};

    std::vector<SlotGridViewModel> backpack_slots_{};
    std::vector<SlotGridViewModel> hotbar_slots_{};
    std::vector<ActionEntryViewModel> action_menu_entries_{};

    std::unique_ptr<ItemTooltipUI> tooltip_ui_{};
    int hovered_slot_index_{-1};
    int hovered_hotbar_index_{-1};

    Rml::String detail_name_{};
    Rml::String detail_category_{};
    Rml::String detail_description_{};
    bool has_detail_{false};
    SelectedSlot selected_slot_{};

    SlotGridDragState drag_state_{};

    Rml::String action_menu_title_{};
    bool action_menu_visible_{false};
    bool listeners_connected_{false};

public:
    InventoryTabContent(engine::core::Context& context,
                        engine::ui::rmlui::RmlDocumentController& document_controller,
                        entt::registry& game_registry,
                        entt::entity player,
                        game::data::ItemCatalog* item_catalog,
                        uint64_t owner_scene_id);
    ~InventoryTabContent() override;

    [[nodiscard]] bool bindModel(Rml::DataModelConstructor& constructor) override;
    void onActivated() override;
    void onDeactivated() override;
    void update(float delta_time) override;
    [[nodiscard]] bool onCancel() override;

private:
    [[nodiscard]] bool isValidPanelIndex(MenuPanelKind kind, int slot_index) const;
    [[nodiscard]] bool isPanelEmpty(MenuPanelKind kind, int slot_index) const;
    [[nodiscard]] int resolveInventorySlotFromHotbar(int hotbar_index) const;

    void ensureTooltip();
    void disconnectRuntimeListeners();

    void syncFromInventory();
    void syncHotbarFromInventory();
    void refreshSlot(int slot_index);
    void markSlotsDirty();
    void markActionMenuDirty();

    void showTooltipForInventorySlot(int slot_index);
    void showTooltipForHotbarSlot(int hotbar_index);
    void showTooltipForPanel(MenuPanelKind kind, int slot_index);
    void clearTooltip();

    void updateDetailForInventorySlot(int slot_index);
    void updateDetailForHotbarSlot(int hotbar_index);
    void updateDetailForPanel(MenuPanelKind kind, int slot_index);
    void clearDetail();
    void selectSlot(MenuPanelKind kind, int slot_index);
    void clearSelection();
    void clearSelectionAndDetail();

    void clearDragState();
    void closeActionMenu();
    void openBackpackActionMenu(int slot_index);
    void openHotbarActionMenu(int slot_index);
    void openDiscardConfirmForBackpackSlot(int slot_index);
    void showActionMenu(Rml::String title,
                        std::vector<ActionEntryViewModel> entries,
                        std::string_view anchor_grid_id,
                        int anchor_slot_index);
    void positionActionMenuForGridSlot(std::string_view grid_id, int slot_index);
    [[nodiscard]] Rml::Element* findIndexedChildElement(std::string_view parent_id, int child_index) const;
    [[nodiscard]] float measureGridHorizontalGap(std::string_view grid_id) const;
    void executeAction(int action_id);

    void onTrashClicked();
    void onSortClicked();
    void onInventoryChanged(const game::defs::InventoryChanged& evt);
    void onHotbarChanged(const game::defs::HotbarChanged& evt);

    void onSlotFocus(MenuPanelKind kind, int slot_index, Rml::Event& event);
    void onSlotMouseDown(MenuPanelKind kind, int slot_index, Rml::Event& event);
    void onSlotMouseUp(MenuPanelKind kind, int slot_index, Rml::Event& event);
    void onSlotHoverEnter(MenuPanelKind kind, int slot_index, Rml::Event& event);
    void onSlotHoverExit(MenuPanelKind kind, int slot_index, Rml::Event& event);
    void onSlotDragStart(MenuPanelKind kind, int slot_index, Rml::Event& event);
    void onSlotDragDrop(MenuPanelKind kind, int slot_index, Rml::Event& event);
    void onSlotDragEnd(MenuPanelKind kind, int slot_index, Rml::Event& event);

    void onActionEntryClick(int action_id, Rml::Event& event);
};

} // namespace game::ui
