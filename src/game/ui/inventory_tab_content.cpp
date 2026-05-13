#include "game/ui/inventory_tab_content.h"

#include "engine/core/context.h"
#include "engine/ui/rmlui/rml_element_helpers.h"
#include "engine/ui/rmlui/rml_mouse_buttons.h"
#include "engine/ui/rmlui/rml_ui_runtime.h"
#include "game/component/hotbar_component.h"
#include "game/component/inventory_component.h"
#include "game/data/item_catalog.h"
#include "game/defs/commands.h"
#include "game/defs/events.h"
#include "game/ui/item_tooltip_ui.h"

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/DataTypeRegister.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Event.h>
#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace {

using engine::ui::rmlui::setPixelProperty;

constexpr int TOTAL_SLOTS = game::component::InventoryComponent::TOTAL_SLOTS;
constexpr int HOTBAR_SLOTS = game::component::HotbarComponent::SLOT_COUNT;

enum class MenuActionId : int {
    Use = 1,
    Discard,
    Activate,
    Unbind,
    ConfirmDiscard,
    Cancel,
};

[[nodiscard]] const game::component::InventoryComponent*
tryGetInventory(const entt::registry& registry, entt::entity player) {
    return registry.try_get<game::component::InventoryComponent>(player);
}

[[nodiscard]] const game::component::HotbarComponent*
tryGetHotbar(const entt::registry& registry, entt::entity player) {
    return registry.try_get<game::component::HotbarComponent>(player);
}

[[nodiscard]] std::optional<engine::ui::SlotItem> toSlotItem(const game::component::ItemStack& stack) {
    if (stack.empty()) {
        return std::nullopt;
    }

    return engine::ui::SlotItem{
        .item_id = stack.item_id_,
        .count = stack.count_,
    };
}

[[nodiscard]] const game::data::ItemData*
findItemAtInventorySlot(const entt::registry& registry,
                        entt::entity player,
                        const game::data::ItemCatalog* catalog,
                        int slot_index) {
    const auto* inventory = tryGetInventory(registry, player);
    if (!inventory || !catalog || slot_index < 0 || slot_index >= inventory->slotCount()) {
        return nullptr;
    }

    const auto& stack = inventory->slot(slot_index);
    if (stack.empty()) {
        return nullptr;
    }

    return catalog->findItem(stack.item_id_);
}

[[nodiscard]] bool hasUsableItemAtInventorySlot(const entt::registry& registry,
                                                entt::entity player,
                                                const game::data::ItemCatalog* catalog,
                                                int slot_index) {
    const auto* item = findItemAtInventorySlot(registry, player, catalog, slot_index);
    return item && (item->on_use_.has_value() || item->battle_use_.has_value());
}

[[nodiscard]] bool requiresActorTargetAtInventorySlot(const entt::registry& registry,
                                                      entt::entity player,
                                                      const game::data::ItemCatalog* catalog,
                                                      int slot_index) {
    const auto* item = findItemAtInventorySlot(registry, player, catalog, slot_index);
    return item && item->battle_use_.has_value() &&
           item->battle_use_->scope == game::data::Scope::OneAlly;
}

} // namespace

namespace game::ui {

InventoryTabContent::InventoryTabContent(engine::core::Context& context,
                                         engine::ui::rmlui::RmlDocumentController& document_controller,
                                         entt::registry& game_registry,
                                         entt::entity player,
                                         game::data::ItemCatalog* item_catalog,
                                         uint64_t owner_scene_id)
    : context_(context),
      document_controller_(document_controller),
      game_registry_(game_registry),
      player_(player),
      item_catalog_(item_catalog),
      owner_scene_id_(owner_scene_id) {
    backpack_slots_.resize(TOTAL_SLOTS);
    for (int i = 0; i < TOTAL_SLOTS; ++i) {
        backpack_slots_[i].slot_index = i;
    }

    hotbar_slots_.resize(HOTBAR_SLOTS);
    for (int i = 0; i < HOTBAR_SLOTS; ++i) {
        hotbar_slots_[i].slot_index = i;
        hotbar_slots_[i].label = std::to_string((i + 1) % 10);
    }
}

InventoryTabContent::~InventoryTabContent() {
    disconnectRuntimeListeners();
}

bool InventoryTabContent::bindModel(Rml::DataModelConstructor& constructor) {
    if (auto action_handle = constructor.RegisterStruct<ActionEntryViewModel>()) {
        action_handle.RegisterMember("action_id", &ActionEntryViewModel::action_id);
        action_handle.RegisterMember("label", &ActionEntryViewModel::label);
        action_handle.RegisterMember("is_destructive", &ActionEntryViewModel::is_destructive);
    } else {
        spdlog::error("InventoryTabContent: RegisterStruct<ActionEntryViewModel> 失败。");
        return false;
    }

    if (!constructor.RegisterArray<decltype(action_menu_entries_)>()) {
        spdlog::error("InventoryTabContent: RegisterArray<ActionEntryViewModel> 失败。");
        return false;
    }

    if (!constructor.Bind("backpack_slots", &backpack_slots_) ||
        !constructor.Bind("hotbar_slots", &hotbar_slots_) ||
        !constructor.Bind("action_menu_entries", &action_menu_entries_)) {
        spdlog::error("InventoryTabContent: 绑定数组 data model 失败。");
        return false;
    }

    constructor.Bind("detail_name", &detail_name_);
    constructor.Bind("detail_category", &detail_category_);
    constructor.Bind("detail_description", &detail_description_);
    constructor.Bind("has_detail", &has_detail_);
    constructor.Bind("action_menu_title", &action_menu_title_);
    constructor.Bind("action_menu_visible", &action_menu_visible_);

    const SlotGridContextEventHandlers<InventoryTabContent, MenuPanelKind> slot_handlers{
        .on_focus = &InventoryTabContent::onSlotFocus,
        .on_mouse_down = &InventoryTabContent::onSlotMouseDown,
        .on_mouse_up = &InventoryTabContent::onSlotMouseUp,
        .on_hover_enter = &InventoryTabContent::onSlotHoverEnter,
        .on_hover_exit = &InventoryTabContent::onSlotHoverExit,
        .on_drag_start = &InventoryTabContent::onSlotDragStart,
        .on_drag_drop = &InventoryTabContent::onSlotDragDrop,
        .on_drag_end = &InventoryTabContent::onSlotDragEnd,
    };

    if (!bindSlotGridContextEvents(constructor, "bp_slot", this, MenuPanelKind::Backpack, slot_handlers) ||
        !bindSlotGridContextEvents(constructor, "hb_slot", this, MenuPanelKind::Hotbar, slot_handlers) ||
        !constructor.BindEventCallback(
            "action_entry_click",
            [this](Rml::DataModelHandle, Rml::Event& event, const Rml::VariantList& arguments) {
                onActionEntryClick(getSingleIntArgument(arguments), event);
            }) ||
        !document_controller_.bindSimpleEvent(constructor, "trash", [this] { onTrashClicked(); }) ||
        !document_controller_.bindSimpleEvent(constructor, "sort", [this] { onSortClicked(); })) {
        spdlog::error("InventoryTabContent: 绑定 event 回调失败。");
        return false;
    }

    return true;
}

void InventoryTabContent::onActivated() {
    if (!listeners_connected_) {
        context_.getDispatcher().sink<game::defs::InventoryChanged>()
            .connect<&InventoryTabContent::onInventoryChanged>(this);
        context_.getDispatcher().sink<game::defs::HotbarChanged>()
            .connect<&InventoryTabContent::onHotbarChanged>(this);
        listeners_connected_ = true;
    }

    ensureTooltip();
    syncFromInventory();
    syncHotbarFromInventory();
    document_controller_.markAllDirty();
}

void InventoryTabContent::onDeactivated() {
    disconnectRuntimeListeners();
    closeActionMenu();
    clearTooltip();
    clearSelectionAndDetail();
    clearDragState();
}

void InventoryTabContent::update(float delta_time) {
    if (tooltip_ui_) {
        tooltip_ui_->update(delta_time);
    }
}

bool InventoryTabContent::onCancel() {
    if (action_menu_visible_) {
        closeActionMenu();
        return true;
    }

    return false;
}

void InventoryTabContent::setActorTargetRequestHandler(ActorTargetRequestHandler handler) {
    actor_target_request_handler_ = std::move(handler);
}

bool InventoryTabContent::isValidPanelIndex(MenuPanelKind kind, int slot_index) const {
    switch (kind) {
        case MenuPanelKind::Backpack:
            return slot_index >= 0 && slot_index < TOTAL_SLOTS;
        case MenuPanelKind::Hotbar:
            return slot_index >= 0 && slot_index < HOTBAR_SLOTS;
        case MenuPanelKind::None:
        default:
            return false;
    }
}

bool InventoryTabContent::isPanelEmpty(MenuPanelKind kind, int slot_index) const {
    switch (kind) {
        case MenuPanelKind::Backpack: {
            const auto* inventory = tryGetInventory(game_registry_, player_);
            return !inventory || slot_index >= inventory->slotCount() || inventory->slot(slot_index).empty();
        }
        case MenuPanelKind::Hotbar: {
            const auto* hotbar = tryGetHotbar(game_registry_, player_);
            return !hotbar || hotbar->slot(slot_index).empty();
        }
        case MenuPanelKind::None:
        default:
            return true;
    }
}

int InventoryTabContent::resolveInventorySlotFromHotbar(int hotbar_index) const {
    const auto* hotbar = tryGetHotbar(game_registry_, player_);
    if (!hotbar || hotbar_index < 0 || hotbar_index >= HOTBAR_SLOTS || hotbar->slot(hotbar_index).empty()) {
        return -1;
    }

    return hotbar->slot(hotbar_index).inventory_slot_index_;
}

int InventoryTabContent::resolveInventorySlotForPanel(MenuPanelKind kind, int slot_index) const {
    switch (kind) {
        case MenuPanelKind::Backpack:
            return isValidPanelIndex(kind, slot_index) ? slot_index : -1;
        case MenuPanelKind::Hotbar:
            return resolveInventorySlotFromHotbar(slot_index);
        case MenuPanelKind::None:
        default:
            return -1;
    }
}

const game::data::ItemData* InventoryTabContent::resolveItemForPanel(MenuPanelKind kind, int slot_index) const {
    const int inventory_slot = resolveInventorySlotForPanel(kind, slot_index);
    if (inventory_slot < 0) {
        return nullptr;
    }

    return findItemAtInventorySlot(game_registry_, player_, item_catalog_, inventory_slot);
}

void InventoryTabContent::ensureTooltip() {
    if (!tooltip_ui_) {
        tooltip_ui_ = std::make_unique<ItemTooltipUI>(context_, owner_scene_id_);
    }
}

void InventoryTabContent::disconnectRuntimeListeners() {
    if (!listeners_connected_) {
        return;
    }

    context_.getDispatcher().sink<game::defs::InventoryChanged>()
        .disconnect<&InventoryTabContent::onInventoryChanged>(this);
    context_.getDispatcher().sink<game::defs::HotbarChanged>()
        .disconnect<&InventoryTabContent::onHotbarChanged>(this);
    listeners_connected_ = false;
}

void InventoryTabContent::syncFromInventory() {
    const auto* inventory = tryGetInventory(game_registry_, player_);
    if (!inventory) {
        return;
    }

    for (int i = 0; i < TOTAL_SLOTS; ++i) {
        auto& vm = backpack_slots_[i];
        vm.slot_index = i;
        populateSlotGridViewModel(
            vm,
            toSlotItem(inventory->slot(i)),
            item_catalog_,
            SlotGridViewModelOptions{
                .can_drag = true,
                .is_selected = (selected_slot_.isBackpack() && selected_slot_.index == i),
            });
    }
}

void InventoryTabContent::syncHotbarFromInventory() {
    const auto* hotbar = tryGetHotbar(game_registry_, player_);
    const auto* inventory = tryGetInventory(game_registry_, player_);

    for (int i = 0; i < HOTBAR_SLOTS; ++i) {
        auto& vm = hotbar_slots_[i];
        vm.slot_index = i;
        // populateSlotGridViewModel 会覆写 vm.label，需提前拷贝。
        // string_view 指向此副本，不受后续 vm 修改影响。
        const Rml::String label = vm.label;

        std::optional<engine::ui::SlotItem> slot_item;
        bool is_active = false;
        if (hotbar && inventory && !hotbar->slot(i).empty()) {
            const int inventory_slot = hotbar->slot(i).inventory_slot_index_;
            if (inventory_slot >= 0 && inventory_slot < inventory->slotCount()) {
                slot_item = toSlotItem(inventory->slot(inventory_slot));
                is_active = (hotbar->active_slot_index_ == i);
            }
        }

        populateSlotGridViewModel(
            vm,
            slot_item,
            item_catalog_,
            SlotGridViewModelOptions{
                .can_drag = slot_item.has_value(),
                .is_selected = (selected_slot_.isHotbar() && selected_slot_.index == i),
                .is_active = is_active,
                .label = std::string_view{label.data(), label.size()},
            });
    }
}

void InventoryTabContent::refreshSlot(int slot_index) {
    if (slot_index < 0 || slot_index >= TOTAL_SLOTS) {
        return;
    }

    const auto* inventory = tryGetInventory(game_registry_, player_);
    if (!inventory) {
        return;
    }

    auto& vm = backpack_slots_[slot_index];
    vm.slot_index = slot_index;
    populateSlotGridViewModel(
        vm,
        toSlotItem(inventory->slot(slot_index)),
        item_catalog_,
        SlotGridViewModelOptions{
            .can_drag = true,
            .is_selected = (selected_slot_.isBackpack() && selected_slot_.index == slot_index),
        });
}

void InventoryTabContent::markSlotsDirty() {
    if (!document_controller_.isModelValid()) {
        return;
    }

    document_controller_.markDirty("backpack_slots");
    document_controller_.markDirty("hotbar_slots");
}

void InventoryTabContent::markActionMenuDirty() {
    if (!document_controller_.isModelValid()) {
        return;
    }

    document_controller_.markDirty("action_menu_entries");
    document_controller_.markDirty("action_menu_title");
    document_controller_.markDirty("action_menu_visible");
}

void InventoryTabContent::showTooltipForPanel(MenuPanelKind kind, int slot_index) {
    if (!tooltip_ui_ || !item_catalog_ || drag_state_.active || action_menu_visible_) {
        return;
    }

    const auto* item = resolveItemForPanel(kind, slot_index);
    if (!item) {
        clearTooltip();
        return;
    }

    tooltip_ui_->showItem(item->display_name_, item->category_str_, item->description_);
}

void InventoryTabContent::clearTooltip() {
    hovered_slot_index_ = -1;
    hovered_hotbar_index_ = -1;
    if (tooltip_ui_) {
        tooltip_ui_->hideTooltip();
    }
}

void InventoryTabContent::setDetailFromItem(const game::data::ItemData& item) {
    detail_name_ = item.display_name_;
    detail_category_ = item.category_str_;
    detail_description_ = item.description_;
    has_detail_ = true;
    document_controller_.markDirty("detail_name");
    document_controller_.markDirty("detail_category");
    document_controller_.markDirty("detail_description");
    document_controller_.markDirty("has_detail");
}

void InventoryTabContent::updateDetailForPanel(MenuPanelKind kind, int slot_index) {
    const auto* item = resolveItemForPanel(kind, slot_index);
    if (!item) {
        clearDetail();
        return;
    }

    setDetailFromItem(*item);
}

void InventoryTabContent::clearDetail() {
    if (!has_detail_) {
        return;
    }

    has_detail_ = false;
    detail_name_.clear();
    detail_category_.clear();
    detail_description_.clear();
    document_controller_.markDirty("detail_name");
    document_controller_.markDirty("detail_category");
    document_controller_.markDirty("detail_description");
    document_controller_.markDirty("has_detail");
}

void InventoryTabContent::selectSlot(MenuPanelKind kind, int slot_index) {
    clearSelection();

    if (!isValidPanelIndex(kind, slot_index)) {
        markSlotsDirty();
        return;
    }

    switch (kind) {
        case MenuPanelKind::Backpack:
            backpack_slots_[slot_index].is_selected = true;
            break;
        case MenuPanelKind::Hotbar:
            hotbar_slots_[slot_index].is_selected = true;
            break;
        case MenuPanelKind::None:
        default:
            break;
    }

    selected_slot_ = SelectedSlot{
        .panel = kind,
        .index = slot_index,
    };
    markSlotsDirty();
}

void InventoryTabContent::clearSelection() {
    if (!selected_slot_.valid()) {
        return;
    }

    if (selected_slot_.isBackpack() && selected_slot_.index >= 0 && selected_slot_.index < TOTAL_SLOTS) {
        backpack_slots_[selected_slot_.index].is_selected = false;
    } else if (selected_slot_.isHotbar() && selected_slot_.index >= 0 && selected_slot_.index < HOTBAR_SLOTS) {
        hotbar_slots_[selected_slot_.index].is_selected = false;
    }

    selected_slot_.clear();
}

void InventoryTabContent::clearSelectionAndDetail() {
    clearSelection();
    markSlotsDirty();
    clearDetail();
}

void InventoryTabContent::clearDragState() {
    drag_state_.clear();
}

void InventoryTabContent::closeActionMenu() {
    if (!action_menu_visible_) {
        return;
    }

    // Let RmlUi remove the action-menu subtree via data-if before replacing the
    // backing array. Clearing the vector in the same update can make stale
    // data-for instances briefly query action_menu_entries[n], which shows up
    // in the debugger as "Data array index out of bounds".
    action_menu_visible_ = false;
    markActionMenuDirty();
}

Rml::Element* InventoryTabContent::findIndexedChildElement(std::string_view parent_id, int child_index) const {
    auto* document = document_controller_.document();
    if (!document || child_index < 0) {
        return nullptr;
    }

    auto* parent = document->GetElementById(Rml::String{parent_id.data(), parent_id.size()});
    if (!parent || child_index >= parent->GetNumChildren()) {
        return nullptr;
    }

    return parent->GetChild(child_index);
}

float InventoryTabContent::measureGridHorizontalGap(std::string_view grid_id) const {
    auto* first_slot = findIndexedChildElement(grid_id, 0);
    auto* second_slot = findIndexedChildElement(grid_id, 1);
    if (!first_slot || !second_slot) {
        return 0.0F;
    }

    return std::max(0.0F, second_slot->GetAbsoluteLeft() - first_slot->GetAbsoluteLeft() - first_slot->GetOffsetWidth());
}

void InventoryTabContent::positionActionMenuForGridSlot(std::string_view grid_id, int slot_index) {
    auto* document = document_controller_.document();
    if (!document || !action_menu_visible_) {
        return;
    }

    auto* slot_region = document->GetElementById("slot-region");
    auto* action_menu = document->GetElementById("action-menu");
    auto* anchor_slot = findIndexedChildElement(grid_id, slot_index);
    if (!slot_region || !action_menu || !anchor_slot) {
        return;
    }

    const float gap = measureGridHorizontalGap(grid_id);
    const float region_left = slot_region->GetAbsoluteLeft();
    const float region_top = slot_region->GetAbsoluteTop();
    const float region_width = slot_region->GetOffsetWidth();
    const float region_height = slot_region->GetOffsetHeight();

    const float anchor_left = anchor_slot->GetAbsoluteLeft() - region_left;
    const float anchor_top = anchor_slot->GetAbsoluteTop() - region_top;
    const float anchor_width = anchor_slot->GetOffsetWidth();
    const float menu_width = action_menu->GetOffsetWidth();
    const float menu_height = action_menu->GetOffsetHeight();

    float left = anchor_left + anchor_width + gap;
    if (left + menu_width > region_width) {
        left = anchor_left - menu_width - gap;
    }

    left = std::clamp(left, 0.0F, std::max(0.0F, region_width - menu_width));
    const float top = std::clamp(anchor_top, 0.0F, std::max(0.0F, region_height - menu_height));

    setPixelProperty(action_menu, "left", left);
    setPixelProperty(action_menu, "top", top);
}

void InventoryTabContent::showActionMenu(Rml::String title,
                                         std::vector<ActionEntryViewModel> entries,
                                         std::string_view anchor_grid_id,
                                         int anchor_slot_index) {
    if (entries.empty()) {
        closeActionMenu();
        return;
    }

    action_menu_entries_ = std::move(entries);
    action_menu_title_ = std::move(title);
    action_menu_visible_ = true;
    markActionMenuDirty();
    clearTooltip();

    if (auto* runtime = context_.getRmlUi()) {
        runtime->update();
        positionActionMenuForGridSlot(anchor_grid_id, anchor_slot_index);
    }
}

void InventoryTabContent::openBackpackActionMenu(int slot_index) {
    const auto* inventory = tryGetInventory(game_registry_, player_);
    if (!inventory || slot_index < 0 || slot_index >= inventory->slotCount() || inventory->slot(slot_index).empty()) {
        closeActionMenu();
        return;
    }

    std::vector<ActionEntryViewModel> entries;
    if (hasUsableItemAtInventorySlot(game_registry_, player_, item_catalog_, slot_index)) {
        entries.push_back(ActionEntryViewModel{
            .action_id = static_cast<int>(MenuActionId::Use),
            .label = "Use",
            .is_destructive = false,
        });
    }
    entries.push_back(ActionEntryViewModel{
        .action_id = static_cast<int>(MenuActionId::Discard),
        .label = "Discard",
        .is_destructive = true,
    });
    entries.push_back(ActionEntryViewModel{
        .action_id = static_cast<int>(MenuActionId::Cancel),
        .label = "Cancel",
        .is_destructive = false,
    });

    const auto* item = findItemAtInventorySlot(game_registry_, player_, item_catalog_, slot_index);
    showActionMenu(item ? item->display_name_ : Rml::String{"Backpack"}, std::move(entries), "backpack-grid", slot_index);
}

void InventoryTabContent::openHotbarActionMenu(int slot_index) {
    const auto* hotbar = tryGetHotbar(game_registry_, player_);
    if (!hotbar || slot_index < 0 || slot_index >= HOTBAR_SLOTS || hotbar->slot(slot_index).empty()) {
        closeActionMenu();
        return;
    }

    const int inventory_slot = hotbar->slot(slot_index).inventory_slot_index_;

    std::vector<ActionEntryViewModel> entries;
    entries.push_back(ActionEntryViewModel{
        .action_id = static_cast<int>(MenuActionId::Activate),
        .label = "Activate",
        .is_destructive = false,
    });
    if (hasUsableItemAtInventorySlot(game_registry_, player_, item_catalog_, inventory_slot)) {
        entries.push_back(ActionEntryViewModel{
            .action_id = static_cast<int>(MenuActionId::Use),
            .label = "Use",
            .is_destructive = false,
        });
    }
    entries.push_back(ActionEntryViewModel{
        .action_id = static_cast<int>(MenuActionId::Unbind),
        .label = "Unbind",
        .is_destructive = false,
    });
    entries.push_back(ActionEntryViewModel{
        .action_id = static_cast<int>(MenuActionId::Cancel),
        .label = "Cancel",
        .is_destructive = false,
    });

    const auto* item = findItemAtInventorySlot(game_registry_, player_, item_catalog_, inventory_slot);
    showActionMenu(item ? item->display_name_ : Rml::String{"Hotbar"}, std::move(entries), "hotbar-grid", slot_index);
}

void InventoryTabContent::openDiscardConfirmForBackpackSlot(int slot_index) {
    const auto* inventory = tryGetInventory(game_registry_, player_);
    if (!inventory || slot_index < 0 || slot_index >= inventory->slotCount()) {
        return;
    }

    const auto& stack = inventory->slot(slot_index);
    if (stack.empty()) {
        return;
    }

    std::vector<ActionEntryViewModel> entries;
    entries.push_back(ActionEntryViewModel{
        .action_id = static_cast<int>(MenuActionId::ConfirmDiscard),
        .label = "Discard",
        .is_destructive = true,
    });
    entries.push_back(ActionEntryViewModel{
        .action_id = static_cast<int>(MenuActionId::Cancel),
        .label = "Cancel",
        .is_destructive = false,
    });

    Rml::String title{"Discard stack?"};
    if (const auto* item = item_catalog_ ? item_catalog_->findItem(stack.item_id_) : nullptr) {
        title = "Discard " + item->display_name_ + " x" + std::to_string(stack.count_) + "?";
    }

    showActionMenu(std::move(title), std::move(entries), "backpack-grid", slot_index);
}

void InventoryTabContent::executeAction(int action_id) {
    if (player_ == entt::null) {
        closeActionMenu();
        return;
    }

    switch (static_cast<MenuActionId>(action_id)) {
        case MenuActionId::Use: {
            int inventory_slot = -1;
            if (selected_slot_.isBackpack()) {
                inventory_slot = selected_slot_.index;
            } else if (selected_slot_.isHotbar()) {
                inventory_slot = resolveInventorySlotFromHotbar(selected_slot_.index);
            }

            closeActionMenu();
            if (inventory_slot >= 0) {
                if (requiresActorTargetAtInventorySlot(game_registry_, player_, item_catalog_, inventory_slot) &&
                    actor_target_request_handler_) {
                    actor_target_request_handler_(inventory_slot);
                } else {
                    context_.getDispatcher().trigger(game::defs::UseItemCommand{player_, inventory_slot, 1, false});
                }
            }
            break;
        }
        case MenuActionId::Discard:
            if (selected_slot_.isBackpack()) {
                openDiscardConfirmForBackpackSlot(selected_slot_.index);
            }
            break;
        case MenuActionId::Activate:
            closeActionMenu();
            if (selected_slot_.isHotbar()) {
                context_.getDispatcher().trigger(game::defs::HotbarActivateCommand{player_, selected_slot_.index});
            }
            break;
        case MenuActionId::Unbind:
            closeActionMenu();
            if (selected_slot_.isHotbar()) {
                context_.getDispatcher().trigger(game::defs::HotbarUnbindCommand{player_, selected_slot_.index});
            }
            break;
        case MenuActionId::ConfirmDiscard: {
            const auto* inventory = tryGetInventory(game_registry_, player_);
            if (!inventory || !selected_slot_.isBackpack() || selected_slot_.index >= inventory->slotCount()) {
                closeActionMenu();
                break;
            }

            const auto& stack = inventory->slot(selected_slot_.index);
            closeActionMenu();
            if (!stack.empty()) {
                context_.getDispatcher().trigger(game::defs::RemoveItemCommand{
                    player_, stack.item_id_, stack.count_, selected_slot_.index});
            }
            break;
        }
        case MenuActionId::Cancel:
        default:
            closeActionMenu();
            break;
    }
}

void InventoryTabContent::onTrashClicked() {
    if (drag_state_.active || !selected_slot_.isBackpack()) {
        return;
    }

    const auto* inventory = tryGetInventory(game_registry_, player_);
    if (!inventory || selected_slot_.index >= inventory->slotCount() || inventory->slot(selected_slot_.index).empty()) {
        return;
    }

    openDiscardConfirmForBackpackSlot(selected_slot_.index);
}

void InventoryTabContent::onSortClicked() {
    if (drag_state_.active || player_ == entt::null) {
        return;
    }

    closeActionMenu();
    clearTooltip();
    clearSelectionAndDetail();
    context_.getDispatcher().trigger(game::defs::InventorySortCommand{player_});
}

void InventoryTabContent::onInventoryChanged(const game::defs::InventoryChanged& evt) {
    if (evt.target != player_) {
        return;
    }

    if (evt.full_sync) {
        syncFromInventory();
        closeActionMenu();
    } else {
        for (const auto& update : evt.slots) {
            refreshSlot(update.slot_index);
        }
    }
    document_controller_.markDirty("backpack_slots");

    if (selected_slot_.isBackpack()) {
        updateDetailForPanel(MenuPanelKind::Backpack, selected_slot_.index);
        const auto* inventory = tryGetInventory(game_registry_, player_);
        if (action_menu_visible_ &&
            (!inventory || selected_slot_.index >= inventory->slotCount() || inventory->slot(selected_slot_.index).empty())) {
            closeActionMenu();
        }
    } else if (selected_slot_.isHotbar()) {
        updateDetailForPanel(MenuPanelKind::Hotbar, selected_slot_.index);
    }
}

void InventoryTabContent::onHotbarChanged(const game::defs::HotbarChanged& evt) {
    if (evt.target != player_) {
        return;
    }

    syncHotbarFromInventory();
    document_controller_.markDirty("hotbar_slots");

    if (selected_slot_.isHotbar()) {
        updateDetailForPanel(MenuPanelKind::Hotbar, selected_slot_.index);

        const auto* hotbar = tryGetHotbar(game_registry_, player_);
        if (action_menu_visible_ &&
            (!hotbar || selected_slot_.index >= HOTBAR_SLOTS || hotbar->slot(selected_slot_.index).empty())) {
            closeActionMenu();
        }
    }
}

void InventoryTabContent::onSlotFocus(MenuPanelKind kind, int slot_index, Rml::Event& /*event*/) {
    if (!isValidPanelIndex(kind, slot_index) || drag_state_.active || action_menu_visible_) {
        return;
    }

    selectSlot(kind, slot_index);
    updateDetailForPanel(kind, slot_index);
}

void InventoryTabContent::onSlotMouseDown(MenuPanelKind kind, int slot_index, Rml::Event& event) {
    if (!isValidPanelIndex(kind, slot_index) || drag_state_.active) {
        return;
    }

    const int button = event.GetParameter("button", -1);
    if (!engine::ui::rmlui::isSecondaryMouseButton(button)) {
        return;
    }

    event.StopPropagation();
    selectSlot(kind, slot_index);
    updateDetailForPanel(kind, slot_index);

    if (isPanelEmpty(kind, slot_index)) {
        closeActionMenu();
        return;
    }

    if (kind == MenuPanelKind::Backpack) {
        openBackpackActionMenu(slot_index);
    } else if (kind == MenuPanelKind::Hotbar) {
        openHotbarActionMenu(slot_index);
    }
}

void InventoryTabContent::onSlotMouseUp(MenuPanelKind kind, int slot_index, Rml::Event& event) {
    if (!isValidPanelIndex(kind, slot_index) || drag_state_.active) {
        return;
    }

    const int button = event.GetParameter("button", -1);
    if (!engine::ui::rmlui::isPrimaryMouseButton(button)) {
        return;
    }

    event.StopPropagation();
    selectSlot(kind, slot_index);
    updateDetailForPanel(kind, slot_index);

    closeActionMenu();

    if (kind == MenuPanelKind::Hotbar && player_ != entt::null) {
        context_.getDispatcher().trigger(game::defs::HotbarActivateCommand{player_, slot_index});
    }
}

void InventoryTabContent::onSlotHoverEnter(MenuPanelKind kind, int slot_index, Rml::Event& event) {
    if (!isValidPanelIndex(kind, slot_index) || action_menu_visible_) {
        return;
    }

    event.StopPropagation();
    if (kind == MenuPanelKind::Backpack) {
        hovered_slot_index_ = slot_index;
        hovered_hotbar_index_ = -1;
    } else if (kind == MenuPanelKind::Hotbar) {
        hovered_hotbar_index_ = slot_index;
        hovered_slot_index_ = -1;
    }
    showTooltipForPanel(kind, slot_index);
}

void InventoryTabContent::onSlotHoverExit(MenuPanelKind kind, int slot_index, Rml::Event& event) {
    if (!isValidPanelIndex(kind, slot_index) || action_menu_visible_) {
        return;
    }

    event.StopPropagation();
    if ((kind == MenuPanelKind::Backpack && hovered_slot_index_ == slot_index) ||
        (kind == MenuPanelKind::Hotbar && hovered_hotbar_index_ == slot_index)) {
        clearTooltip();
    }
}

void InventoryTabContent::onSlotDragStart(MenuPanelKind kind, int slot_index, Rml::Event& event) {
    if (!isValidPanelIndex(kind, slot_index) || player_ == entt::null || isPanelEmpty(kind, slot_index)) {
        return;
    }

    event.StopPropagation();
    closeActionMenu();
    clearTooltip();
    drag_state_.start();
}

void InventoryTabContent::onSlotDragDrop(MenuPanelKind kind, int slot_index, Rml::Event& event) {
    if (!isValidPanelIndex(kind, slot_index) || player_ == entt::null || !drag_state_.active) {
        return;
    }

    const auto drag_info = getSlotGridDragInfo(event);
    if (!drag_info) {
        return;
    }

    event.StopPropagation();
    clearTooltip();
    drag_state_.drop_handled = true;

    if (kind == MenuPanelKind::Backpack) {
        if (drag_info->fromHotbar()) {
            const auto* hotbar = tryGetHotbar(game_registry_, player_);
            if (!hotbar || drag_info->slot_index >= HOTBAR_SLOTS) {
                return;
            }

            const int source_inventory_slot = hotbar->slot(drag_info->slot_index).inventory_slot_index_;
            if (source_inventory_slot >= 0 && source_inventory_slot != slot_index) {
                context_.getDispatcher().trigger(game::defs::InventoryMoveCommand{
                    player_, source_inventory_slot, slot_index, true});
            }
            return;
        }

        if (drag_info->slot_index != slot_index) {
            context_.getDispatcher().trigger(game::defs::InventoryMoveCommand{
                player_, drag_info->slot_index, slot_index, true});
        }
        return;
    }

    if (drag_info->fromHotbar()) {
        if (drag_info->slot_index == slot_index) {
            return;
        }

        const auto* hotbar = tryGetHotbar(game_registry_, player_);
        if (!hotbar) {
            return;
        }

        const int source_inventory_slot = hotbar->slot(drag_info->slot_index).inventory_slot_index_;
        if (hotbar->slot(slot_index).empty()) {
            context_.getDispatcher().trigger(game::defs::HotbarBindCommand{player_, slot_index, source_inventory_slot});
        } else {
            const int target_inventory_slot = hotbar->slot(slot_index).inventory_slot_index_;
            context_.getDispatcher().trigger(game::defs::HotbarBindCommand{player_, slot_index, source_inventory_slot});
            context_.getDispatcher().trigger(game::defs::HotbarBindCommand{
                player_, drag_info->slot_index, target_inventory_slot});
        }
        return;
    }

    context_.getDispatcher().trigger(game::defs::HotbarBindCommand{player_, slot_index, drag_info->slot_index});
}

void InventoryTabContent::onSlotDragEnd(MenuPanelKind kind, int slot_index, Rml::Event& event) {
    const auto drag_info = getSlotGridDragInfo(event);
    if (!drag_state_.active || !drag_info) {
        return;
    }

    if (kind == MenuPanelKind::Backpack) {
        if (!drag_info->fromInventory() || slot_index != drag_info->slot_index) {
            return;
        }

        event.StopPropagation();
        clearDragState();
        return;
    }

    if (!drag_info->fromHotbar() || slot_index != drag_info->slot_index) {
        return;
    }

    event.StopPropagation();
    if (!drag_state_.drop_handled) {
        context_.getDispatcher().trigger(game::defs::HotbarUnbindCommand{player_, drag_info->slot_index});
    }
    clearDragState();
}

void InventoryTabContent::onActionEntryClick(int action_id, Rml::Event& event) {
    event.StopPropagation();
    executeAction(action_id);
}

} // namespace game::ui
