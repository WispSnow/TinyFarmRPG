#pragma once

#include "engine/scene/scene.h"
#include "engine/ui/rmlui/rml_document_controller.h"
#include "game/ui/slot_grid_support.h"

#include <RmlUi/Core/DataTypeRegister.h>
#include <RmlUi/Core/Types.h>
#include <entt/entity/entity.hpp>
#include <entt/entity/fwd.hpp>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace engine::core {
enum class State;
}

namespace Rml {
class Element;
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
}

namespace game::scene {

class InventoryMenuScene final : public engine::scene::Scene {
    struct ActionEntryViewModel {
        int action_id{0};
        Rml::String label{};
        bool is_destructive{false};
    };

    entt::registry& game_registry_;
    entt::entity player_{entt::null};
    game::data::ItemCatalog* item_catalog_{nullptr};
    engine::core::State previous_state_{};
    bool context_pushed_{false};

    engine::ui::rmlui::RmlDocumentController document_controller_{};
    Rml::DataTypeRegister type_register_{};

    std::vector<game::ui::SlotGridViewModel> backpack_slots_{};
    std::vector<game::ui::SlotGridViewModel> hotbar_slots_{};
    std::vector<ActionEntryViewModel> action_menu_entries_{};
    bool data_types_registered_{false};

    // Tooltip
    std::unique_ptr<game::ui::ItemTooltipUI> tooltip_ui_{};
    int hovered_slot_index_{-1};
    int hovered_hotbar_index_{-1};

    // Detail panel (driven by focus, not hover)
    Rml::String detail_name_{};
    Rml::String detail_category_{};
    Rml::String detail_description_{};
    bool has_detail_{false};
    int detail_bp_slot_{-1};
    int detail_hb_slot_{-1};

    // Drag state
    game::ui::SlotGridDragState drag_state_{};

    // Action menu
    Rml::String action_menu_title_{};
    bool action_menu_visible_{false};
    Rml::Element* focus_before_action_menu_{nullptr};

    // Character panel
    Rml::String char_name_{"Player"};
    Rml::String char_title_{"Lv.1 Farmer"};
    Rml::String gold_label_{"Gold: --"};
    Rml::String farm_label_{"TinyFarm"};

public:
    InventoryMenuScene(std::string_view name,
                       engine::core::Context& context,
                       entt::registry& game_registry,
                       entt::entity player,
                       game::data::ItemCatalog* item_catalog);
    ~InventoryMenuScene() override;

    bool init() override;
    void update(float delta_time) override;
    void clean() override;

private:
    [[nodiscard]] bool initUI();
    void shutdownUI();
    void disconnectRuntimeListeners();

    void syncFromInventory();
    void syncHotbarFromInventory();
    void syncCharacterPanel();
    void refreshSlot(int slot_index);
    void markSlotsDirty();
    void markActionMenuDirty();

    // Tooltip
    void showTooltipForInventorySlot(int slot_index);
    void showTooltipForHotbarSlot(int hotbar_index);
    void clearTooltip();

    // Detail panel & selection
    void updateDetailForInventorySlot(int slot_index);
    void updateDetailForHotbarSlot(int hotbar_index);
    void clearDetail();
    void selectBpSlot(int slot_index);
    void selectHbSlot(int slot_index);
    void clearSelection();

    // Drag
    void clearDragState();
    void closeActionMenu(bool restore_focus = true);
    void openBackpackActionMenu(int slot_index);
    void openHotbarActionMenu(int slot_index);
    void openDiscardConfirmForBackpackSlot(int slot_index);
    void rememberFocusBeforeActionMenu();
    void showActionMenu(Rml::String title,
                        std::vector<ActionEntryViewModel> entries,
                        std::string_view anchor_grid_id,
                        int anchor_slot_index);
    void positionActionMenuForGridSlot(std::string_view grid_id, int slot_index);
    [[nodiscard]] Rml::Element* findIndexedChildElement(std::string_view parent_id, int child_index) const;
    [[nodiscard]] float measureGridHorizontalGap(std::string_view grid_id) const;
    void executeAction(int action_id);
    void clearSelectionAndDetail();

    // Actions
    bool onMenuCancelPressed();
    void onTrashClicked();
    void onSortClicked();
    void onInventoryChanged(const game::defs::InventoryChanged& evt);
    void onHotbarChanged(const game::defs::HotbarChanged& evt);

    // Backpack slot event callbacks (bound via BindEventCallback)
    void onBpSlotFocus(int slot_index, Rml::Event& event);
    void onBpSlotMouseDown(int slot_index, Rml::Event& event);
    void onBpSlotMouseUp(int slot_index, Rml::Event& event);
    void onBpSlotHoverEnter(int slot_index, Rml::Event& event);
    void onBpSlotHoverExit(int slot_index, Rml::Event& event);
    void onBpSlotDragStart(int slot_index, Rml::Event& event);
    void onBpSlotDragDrop(int slot_index, Rml::Event& event);
    void onBpSlotDragEnd(int slot_index, Rml::Event& event);

    // Hotbar slot event callbacks
    void onHbSlotFocus(int slot_index, Rml::Event& event);
    void onHbSlotMouseDown(int slot_index, Rml::Event& event);
    void onHbSlotMouseUp(int slot_index, Rml::Event& event);
    void onHbSlotHoverEnter(int slot_index, Rml::Event& event);
    void onHbSlotHoverExit(int slot_index, Rml::Event& event);
    void onHbSlotDragStart(int slot_index, Rml::Event& event);
    void onHbSlotDragDrop(int slot_index, Rml::Event& event);
    void onHbSlotDragEnd(int slot_index, Rml::Event& event);

    // Action menu entry callbacks
    void onActionEntryClick(int action_id, Rml::Event& event);
};

} // namespace game::scene
