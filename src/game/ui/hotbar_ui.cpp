#include "hotbar_ui.h"

#include "engine/core/context.h"
#include "engine/ui/rmlui/rml_mouse_buttons.h"
#include "engine/ui/rmlui/rml_ui_runtime.h"
#include "game/component/hotbar_component.h"
#include "game/defs/commands.h"
#include "game/ui/item_tooltip_ui.h"

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/DataTypeRegister.h>
#include <RmlUi/Core/Event.h>
#include <entt/signal/dispatcher.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <string>
#include <string_view>

namespace {

constexpr int SLOT_COUNT = game::component::HotbarComponent::SLOT_COUNT;
constexpr std::string_view DOCUMENT_PATH = "ui/rmlui/hud/hotbar.rml";
constexpr std::string_view MODEL_NAME = "hotbar_ui";

} // namespace

namespace game::ui {

HotbarUI::HotbarUI(engine::ui::rmlui::RmlUiRuntime& runtime,
                   engine::core::Context& context,
                   uint64_t owner_scene_id,
                   game::data::ItemCatalog* catalog)
    : runtime_(runtime),
      context_(context),
      item_catalog_(catalog),
      owner_scene_id_(owner_scene_id) {
    hotbar_slots_.resize(SLOT_COUNT);
    slot_items_.resize(SLOT_COUNT);
    slot_inventory_indices_.assign(SLOT_COUNT, -1);
    refreshAllSlotViewModels();
    if (!initDocument()) {
        spdlog::error("HotbarUI: 初始化失败。");
    }
}

HotbarUI::~HotbarUI() {
    clearTooltip();
    destroyDocument();
}

bool HotbarUI::initDocument() {
    document_controller_.attach(&runtime_, owner_scene_id_);
    auto constructor = document_controller_.createModel(MODEL_NAME, &type_register_);
    if (!constructor) {
        spdlog::error("HotbarUI: 创建 data model 失败。");
        return false;
    }

    if (!ensureDataTypesRegistered(constructor)) {
        spdlog::error("HotbarUI: 注册 hotbar data types 失败。");
        document_controller_.unload();
        return false;
    }

    if (!constructor.Bind("hotbar_slots", &hotbar_slots_)) {
        spdlog::error("HotbarUI: 绑定 hotbar_slots 失败。");
        document_controller_.unload();
        return false;
    }

    if (!bindSlotGridEvents(
            constructor,
            "slot",
            this,
            SlotGridEventHandlers<HotbarUI>{
                .on_mouse_up = &HotbarUI::onSlotMouseUp,
                .on_hover_enter = &HotbarUI::onSlotHoverEnter,
                .on_hover_exit = &HotbarUI::onSlotHoverExit,
                .on_drag_start = &HotbarUI::onSlotDragStart,
                .on_drag_drop = &HotbarUI::onSlotDragDrop,
                .on_drag_end = &HotbarUI::onSlotDragEnd,
            })) {
        spdlog::error("HotbarUI: 绑定 data event 回调失败。");
        document_controller_.unload();
        return false;
    }

    if (!document_controller_.load(DOCUMENT_PATH)) {
        spdlog::error("HotbarUI: 加载 RML 文档失败: {}", DOCUMENT_PATH);
        document_controller_.unload();
        return false;
    }

    document_controller_.markAllDirty();
    if (!visible_ && document_controller_.document()) {
        runtime_.hideDocument(document_controller_.document());
    }
    return true;
}

bool HotbarUI::ensureDataTypesRegistered(Rml::DataModelConstructor& constructor) {
    if (data_types_registered_) {
        return true;
    }

    if (!registerSlotGridViewModelType(constructor)) {
        return false;
    }

    if (!constructor.RegisterArray<decltype(hotbar_slots_)>()) {
        return false;
    }

    data_types_registered_ = true;
    return true;
}

void HotbarUI::destroyDocument() {
    clearDragState();
    document_controller_.unload();
}

bool HotbarUI::isValidSlotIndex(int slot_index) const {
    return slot_index >= 0 && slot_index < SLOT_COUNT;
}

void HotbarUI::refreshAllSlotViewModels() {
    for (int slot_index = 0; slot_index < SLOT_COUNT; ++slot_index) {
        refreshSlotViewModel(slot_index);
    }
}

void HotbarUI::refreshSlotViewModel(int slot_index) {
    if (!isValidSlotIndex(slot_index)) {
        return;
    }

    auto& slot = hotbar_slots_[static_cast<std::size_t>(slot_index)];
    slot.slot_index = slot_index;
    populateSlotGridViewModel(
        slot,
        slot_items_[static_cast<std::size_t>(slot_index)],
        item_catalog_,
        SlotGridViewModelOptions{
            .can_drag = (slot_inventory_indices_[static_cast<std::size_t>(slot_index)] >= 0),
            .is_active = (slot_index == active_slot_index_),
        });
}

void HotbarUI::markSlotsDirty() {
    if (document_controller_.isModelValid()) {
        document_controller_.markDirty("hotbar_slots");
    }
    refreshTooltipForHoveredSlot();
}

std::optional<engine::ui::SlotItem> HotbarUI::getSlotItemData(int slot_index) const {
    if (!isValidSlotIndex(slot_index)) {
        return std::nullopt;
    }
    return slot_items_[static_cast<std::size_t>(slot_index)];
}

void HotbarUI::showTooltipForSlot(int slot_index) {
    if (!tooltip_ui_ || !item_catalog_ || drag_state_.active || !visible_) {
        return;
    }

    const auto item = getSlotItemData(slot_index);
    if (!item || item->item_id == entt::null || item->count <= 0) {
        clearTooltip();
        return;
    }

    const auto* data = item_catalog_->findItem(item->item_id);
    if (!data) {
        clearTooltip();
        return;
    }

    tooltip_ui_->showItem(data->display_name_, data->category_str_, data->description_);
}

void HotbarUI::refreshTooltipForHoveredSlot() {
    if (hovered_slot_index_ < 0) {
        return;
    }
    showTooltipForSlot(hovered_slot_index_);
}

void HotbarUI::clearTooltip() {
    hovered_slot_index_ = -1;
    if (tooltip_ui_) {
        tooltip_ui_->hideTooltip();
    }
}

void HotbarUI::clearDragState() {
    drag_state_.clear();
}

void HotbarUI::setSlotItem(int slot_index, const engine::ui::SlotItem& item) {
    if (!isValidSlotIndex(slot_index)) {
        return;
    }
    slot_items_[static_cast<std::size_t>(slot_index)] = item;
    refreshSlotViewModel(slot_index);
    markSlotsDirty();
}

void HotbarUI::clearSlot(int slot_index) {
    if (!isValidSlotIndex(slot_index)) {
        return;
    }
    slot_items_[static_cast<std::size_t>(slot_index)].reset();
    refreshSlotViewModel(slot_index);
    markSlotsDirty();
}

void HotbarUI::clearAllSlots() {
    for (auto& item : slot_items_) {
        item.reset();
    }
    refreshAllSlotViewModels();
    markSlotsDirty();
}

void HotbarUI::setActiveSlot(int slot_index) {
    if (slot_index < -1 || slot_index >= SLOT_COUNT || slot_index == active_slot_index_) {
        return;
    }

    const int previous_active_slot = active_slot_index_;
    active_slot_index_ = slot_index;

    if (isValidSlotIndex(previous_active_slot)) {
        refreshSlotViewModel(previous_active_slot);
    }
    if (isValidSlotIndex(active_slot_index_)) {
        refreshSlotViewModel(active_slot_index_);
    }

    markSlotsDirty();
}

void HotbarUI::setSlotInventoryIndex(int slot_index, int inventory_index) {
    if (!isValidSlotIndex(slot_index)) {
        return;
    }
    slot_inventory_indices_[static_cast<std::size_t>(slot_index)] = inventory_index;
    refreshSlotViewModel(slot_index);
    markSlotsDirty();
}

void HotbarUI::resetInventoryMappings() {
    std::fill(slot_inventory_indices_.begin(), slot_inventory_indices_.end(), -1);
    refreshAllSlotViewModels();
    markSlotsDirty();
}

void HotbarUI::show() {
    visible_ = true;
    if (auto* document = document_controller_.document()) {
        runtime_.showDocument(document);
    }
}

void HotbarUI::hide() {
    visible_ = false;
    clearTooltip();
    clearDragState();
    if (auto* document = document_controller_.document()) {
        runtime_.hideDocument(document);
    }
}

void HotbarUI::toggle() {
    if (visible_) {
        hide();
    } else {
        show();
    }
}

void HotbarUI::onSlotMouseUp(int slot_index, Rml::Event& event) {
    if (!isValidSlotIndex(slot_index)) {
        return;
    }

    const int button = event.GetParameter("button", -1);
    if (button < 0) {
        return;
    }

    event.StopPropagation();

    if (engine::ui::rmlui::isPrimaryMouseButton(button) && drag_state_.suppress_next_primary_mouse_up) {
        drag_state_.suppress_next_primary_mouse_up = false;
        return;
    }

    if (target_ == entt::null) {
        return;
    }

    if (engine::ui::rmlui::isPrimaryMouseButton(button)) {
        context_.getDispatcher().trigger(game::defs::HotbarActivateCommand{target_, slot_index});
        return;
    }

    if (!engine::ui::rmlui::isSecondaryMouseButton(button)) {
        return;
    }

    const int inventory_index = slot_inventory_indices_[static_cast<std::size_t>(slot_index)];
    if (inventory_index < 0) {
        return;
    }

    const auto slot_item = getSlotItemData(slot_index);
    if (!slot_item || slot_item->item_id == entt::null || slot_item->count <= 0) {
        return;
    }

    bool can_use = false;
    if (item_catalog_) {
        if (const auto* data = item_catalog_->findItem(slot_item->item_id)) {
            can_use = data->on_use_.has_value();
        }
    }

    if (can_use) {
        context_.getDispatcher().trigger(game::defs::UseItemCommand{target_, inventory_index, 1, false});
    }
}

void HotbarUI::onSlotHoverEnter(int slot_index, Rml::Event& event) {
    if (!isValidSlotIndex(slot_index)) {
        return;
    }
    event.StopPropagation();
    hovered_slot_index_ = slot_index;
    showTooltipForSlot(slot_index);
}

void HotbarUI::onSlotHoverExit(int slot_index, Rml::Event& event) {
    if (!isValidSlotIndex(slot_index)) {
        return;
    }
    event.StopPropagation();
    if (hovered_slot_index_ == slot_index) {
        clearTooltip();
    }
}

void HotbarUI::onSlotDragStart(int slot_index, Rml::Event& event) {
    if (!isValidSlotIndex(slot_index) || target_ == entt::null) {
        return;
    }

    const int inventory_index = slot_inventory_indices_[static_cast<std::size_t>(slot_index)];
    const auto slot_item = getSlotItemData(slot_index);
    if (inventory_index < 0 || !slot_item || slot_item->item_id == entt::null || slot_item->count <= 0) {
        return;
    }

    event.StopPropagation();
    drag_state_.startFromHotbar(slot_index, inventory_index);
    clearTooltip();
}

void HotbarUI::onSlotDragDrop(int slot_index, Rml::Event& event) {
    if (!isValidSlotIndex(slot_index) || target_ == entt::null || !drag_state_.active || drag_state_.source_slot_index < 0) {
        return;
    }

    event.StopPropagation();
    clearTooltip();
    drag_state_.drop_handled = true;

    if (slot_index == drag_state_.source_slot_index) {
        context_.getDispatcher().trigger(game::defs::HotbarActivateCommand{target_, slot_index});
        return;
    }

    const int dst_inventory_index = slot_inventory_indices_[static_cast<std::size_t>(slot_index)];
    if (dst_inventory_index >= 0) {
        context_.getDispatcher().trigger(
            game::defs::HotbarBindCommand{target_, slot_index, drag_state_.source_inventory_slot_index});
        context_.getDispatcher().trigger(
            game::defs::HotbarBindCommand{target_, drag_state_.source_slot_index, dst_inventory_index});
    } else {
        context_.getDispatcher().trigger(
            game::defs::HotbarBindCommand{target_, slot_index, drag_state_.source_inventory_slot_index});
        context_.getDispatcher().trigger(game::defs::HotbarUnbindCommand{target_, drag_state_.source_slot_index});
    }

    context_.getDispatcher().trigger(game::defs::HotbarActivateCommand{target_, slot_index});
}

void HotbarUI::onSlotDragEnd(int slot_index, Rml::Event& event) {
    if (!drag_state_.active || slot_index != drag_state_.source_slot_index) {
        return;
    }

    event.StopPropagation();
    if (!drag_state_.drop_handled && target_ != entt::null) {
        context_.getDispatcher().trigger(game::defs::HotbarUnbindCommand{target_, drag_state_.source_slot_index});
    }
    clearDragState();
}

} // namespace game::ui
