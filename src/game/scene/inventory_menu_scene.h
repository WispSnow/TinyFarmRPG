#pragma once

#include "engine/scene/scene.h"
#include "engine/ui/rmlui/rml_data_bridge.h"
#include "engine/ui/rmlui/rml_event_bridge.h"

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
class ElementDocument;
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
    struct SlotViewModel {
        int slot_index{0};
        Rml::String icon_decorator{"none"};
        Rml::String count_text{};
        bool has_item{false};
        bool has_count{false};
        bool can_drag{false};
        bool is_selected{false};
    };

    struct HotbarSlotViewModel {
        int slot_index{0};
        Rml::String icon_decorator{"none"};
        Rml::String count_text{};
        Rml::String label{};
        bool has_item{false};
        bool has_count{false};
        bool is_active{false};
        bool can_drag{false};
        bool is_selected{false};
    };

    entt::registry& game_registry_;
    entt::entity player_{entt::null};
    game::data::ItemCatalog* item_catalog_{nullptr};
    engine::core::State previous_state_{};
    bool context_pushed_{false};

    engine::ui::rmlui::RmlDataBridge data_bridge_{};
    engine::ui::rmlui::RmlEventBridge event_bridge_{};
    Rml::DataTypeRegister type_register_{};
    Rml::ElementDocument* document_{nullptr};
    bool click_listener_registered_{false};

    std::vector<SlotViewModel> backpack_slots_{};
    std::vector<HotbarSlotViewModel> hotbar_slots_{};
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
    bool dragging_{false};
    bool drop_handled_{false};
    bool dragging_from_hotbar_{false};
    int dragging_slot_index_{-1};

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
    void removeEventListeners();
    void disconnectRuntimeListeners();

    void syncFromInventory();
    void syncHotbarFromInventory();
    void refreshSlot(int slot_index);
    void markSlotsDirty();

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

    // Actions
    bool onMenuCancelPressed();
    void onCloseClicked();
    void onInventoryChanged(const game::defs::InventoryChanged& evt);
    void onHotbarChanged(const game::defs::HotbarChanged& evt);

    // Backpack slot event callbacks (bound via BindEventCallback)
    void onBpSlotFocus(int slot_index, Rml::Event& event);
    void onBpSlotHoverEnter(int slot_index, Rml::Event& event);
    void onBpSlotHoverExit(int slot_index, Rml::Event& event);
    void onBpSlotDragStart(int slot_index, Rml::Event& event);
    void onBpSlotDragDrop(int slot_index, Rml::Event& event);
    void onBpSlotDragEnd(int slot_index, Rml::Event& event);

    // Hotbar slot event callbacks
    void onHbSlotFocus(int slot_index, Rml::Event& event);
    void onHbSlotMouseUp(int slot_index, Rml::Event& event);
    void onHbSlotHoverEnter(int slot_index, Rml::Event& event);
    void onHbSlotHoverExit(int slot_index, Rml::Event& event);
    void onHbSlotDragStart(int slot_index, Rml::Event& event);
    void onHbSlotDragDrop(int slot_index, Rml::Event& event);
    void onHbSlotDragEnd(int slot_index, Rml::Event& event);
};

} // namespace game::scene
