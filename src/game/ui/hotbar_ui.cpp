#include "hotbar_ui.h"

#include "engine/core/context.h"
#include "engine/ui/rmlui/rml_ui_layer.h"
#include "game/component/hotbar_component.h"
#include "game/defs/commands.h"
#include "game/ui/inventory_ui.h"
#include "game/ui/item_tooltip_ui.h"
#include "game/ui/rml_item_icon_helpers.h"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/DataTypeRegister.h>
#include <RmlUi/Core/ElementDocument.h>
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

[[nodiscard]] int getSlotArgument(const Rml::VariantList& arguments) {
    return (arguments.size() == 1 ? arguments[0].Get<int>(-1) : -1);
}

} // namespace

namespace game::ui {

HotbarUI::HotbarUI(engine::ui::rmlui::RmlUILayer& layer,
                   engine::core::Context& context,
                   uint64_t owner_scene_id,
                   game::data::ItemCatalog* catalog)
    : layer_(layer),
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
    data_bridge_.destroy();
}

bool HotbarUI::initDocument() {
    auto* rml_context = layer_.getContext();
    if (!rml_context) {
        spdlog::error("HotbarUI: RmlUi context 不可用。");
        return false;
    }

    auto constructor = data_bridge_.create(rml_context, MODEL_NAME, &type_register_);
    if (!constructor) {
        spdlog::error("HotbarUI: 创建 data model 失败。");
        return false;
    }

    if (!ensureDataTypesRegistered(constructor)) {
        spdlog::error("HotbarUI: 注册 hotbar data types 失败。");
        data_bridge_.destroy();
        return false;
    }

    if (!constructor.Bind("hotbar_slots", &hotbar_slots_)) {
        spdlog::error("HotbarUI: 绑定 hotbar_slots 失败。");
        data_bridge_.destroy();
        return false;
    }

    const auto bind_slot_event = [this, &constructor](const char* name, void (HotbarUI::*handler)(int, Rml::Event&)) {
        return constructor.BindEventCallback(name,
            [this, handler](Rml::DataModelHandle, Rml::Event& event, const Rml::VariantList& arguments) {
                (this->*handler)(getSlotArgument(arguments), event);
            });
    };

    if (!bind_slot_event("slot_mouse_down", &HotbarUI::onSlotMouseDown) ||
        !bind_slot_event("slot_mouse_up", &HotbarUI::onSlotMouseUp) ||
        !bind_slot_event("slot_hover_enter", &HotbarUI::onSlotHoverEnter) ||
        !bind_slot_event("slot_hover_exit", &HotbarUI::onSlotHoverExit) ||
        !bind_slot_event("slot_drag_start", &HotbarUI::onSlotDragStart) ||
        !bind_slot_event("slot_drag_drop", &HotbarUI::onSlotDragDrop) ||
        !bind_slot_event("slot_drag_end", &HotbarUI::onSlotDragEnd)) {
        spdlog::error("HotbarUI: 绑定 data event 回调失败。");
        data_bridge_.destroy();
        return false;
    }

    document_ = layer_.loadDocument(DOCUMENT_PATH, owner_scene_id_);
    if (!document_) {
        spdlog::error("HotbarUI: 加载 RML 文档失败: {}", DOCUMENT_PATH);
        data_bridge_.destroy();
        return false;
    }

    data_bridge_.markAllDirty();
    if (!visible_) {
        layer_.hideDocument(document_);
    }
    return true;
}

bool HotbarUI::ensureDataTypesRegistered(Rml::DataModelConstructor& constructor) {
    if (data_types_registered_) {
        return true;
    }

    if (auto slot_handle = constructor.RegisterStruct<HotbarSlotViewModel>()) {
        slot_handle.RegisterMember("slot_index", &HotbarSlotViewModel::slot_index);
        slot_handle.RegisterMember("icon_decorator", &HotbarSlotViewModel::icon_decorator);
        slot_handle.RegisterMember("count_text", &HotbarSlotViewModel::count_text);
        slot_handle.RegisterMember("has_item", &HotbarSlotViewModel::has_item);
        slot_handle.RegisterMember("has_count", &HotbarSlotViewModel::has_count);
        slot_handle.RegisterMember("is_active", &HotbarSlotViewModel::is_active);
        slot_handle.RegisterMember("is_bound", &HotbarSlotViewModel::is_bound);
        slot_handle.RegisterMember("can_drag", &HotbarSlotViewModel::can_drag);
    } else {
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
    if (document_) {
        layer_.unloadDocument(document_);
        document_ = nullptr;
    }
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
    slot.is_active = (slot_index == active_slot_index_);
    slot.is_bound = slot_inventory_indices_[static_cast<std::size_t>(slot_index)] >= 0;
    slot.icon_decorator.clear();
    slot.count_text.clear();
    slot.has_item = false;
    slot.has_count = false;
    slot.can_drag = false;

    const auto& item = slot_items_[static_cast<std::size_t>(slot_index)];
    if (!item || item->item_id == entt::null || item->count <= 0) {
        return;
    }

    slot.icon_decorator = buildItemIconDecorator(item_catalog_, item->item_id);
    slot.has_item = !slot.icon_decorator.empty();
    slot.can_drag = slot.has_item && slot.is_bound;
    if (item->count > 1 && slot.has_item) {
        slot.count_text = std::to_string(item->count);
        slot.has_count = true;
    }
}

void HotbarUI::markSlotsDirty() {
    if (data_bridge_.isValid()) {
        data_bridge_.markDirty("hotbar_slots");
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
    if (!tooltip_ui_ || !item_catalog_ || dragging_ || !visible_ || (inventory_ui_ && inventory_ui_->isDragging())) {
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
    dragging_ = false;
    drop_handled_ = false;
    suppress_next_primary_mouse_up_ = false;
    dragging_slot_ = -1;
    dragging_inventory_slot_ = -1;
    dragging_item_.reset();
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
    if (document_) {
        layer_.showDocument(document_);
    }
}

void HotbarUI::hide() {
    visible_ = false;
    clearTooltip();
    clearDragState();
    if (document_) {
        layer_.hideDocument(document_);
    }
}

void HotbarUI::toggle() {
    if (visible_) {
        hide();
    } else {
        show();
    }
}

void HotbarUI::onSlotMouseDown(int slot_index, Rml::Event& event) {
    if (!isValidSlotIndex(slot_index)) {
        return;
    }
    event.StopPropagation();
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

    if (button == 0 && suppress_next_primary_mouse_up_) {
        suppress_next_primary_mouse_up_ = false;
        return;
    }

    if (button == 0 && inventory_ui_ && inventory_ui_->isDragging()) {
        return;
    }

    if (target_ == entt::null) {
        return;
    }

    if (button == 0) {
        context_.getDispatcher().trigger(game::defs::HotbarActivateCommand{target_, slot_index});
        return;
    }

    if (button != 2) {
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
    dragging_ = true;
    drop_handled_ = false;
    suppress_next_primary_mouse_up_ = true;
    dragging_slot_ = slot_index;
    dragging_inventory_slot_ = inventory_index;
    dragging_item_ = slot_item;
    clearTooltip();
}

void HotbarUI::onSlotDragDrop(int slot_index, Rml::Event& event) {
    if (!isValidSlotIndex(slot_index) || target_ == entt::null) {
        return;
    }

    if (dragging_) {
        if (dragging_slot_ < 0) {
            return;
        }

        event.StopPropagation();
        clearTooltip();
        drop_handled_ = true;

        if (slot_index == dragging_slot_) {
            context_.getDispatcher().trigger(game::defs::HotbarActivateCommand{target_, slot_index});
            return;
        }

        const int dst_inventory_index = slot_inventory_indices_[static_cast<std::size_t>(slot_index)];
        if (dst_inventory_index >= 0) {
            context_.getDispatcher().trigger(game::defs::HotbarBindCommand{target_, slot_index, dragging_inventory_slot_});
            context_.getDispatcher().trigger(game::defs::HotbarBindCommand{target_, dragging_slot_, dst_inventory_index});
        } else {
            context_.getDispatcher().trigger(game::defs::HotbarBindCommand{target_, slot_index, dragging_inventory_slot_});
            context_.getDispatcher().trigger(game::defs::HotbarUnbindCommand{target_, dragging_slot_});
        }

        context_.getDispatcher().trigger(game::defs::HotbarActivateCommand{target_, slot_index});
        return;
    }

    if (!inventory_ui_ || !inventory_ui_->isDragging()) {
        return;
    }

    const int source_inventory_index = inventory_ui_->getDragInventoryIndex();
    if (source_inventory_index < 0) {
        return;
    }

    event.StopPropagation();
    clearTooltip();
    suppress_next_primary_mouse_up_ = true;
    context_.getDispatcher().trigger(game::defs::HotbarBindCommand{target_, slot_index, source_inventory_index});
    inventory_ui_->notifyExternalDropHandled();
}

void HotbarUI::onSlotDragEnd(int slot_index, Rml::Event& event) {
    if (!dragging_ || slot_index != dragging_slot_) {
        return;
    }

    event.StopPropagation();
    if (!drop_handled_ && target_ != entt::null) {
        context_.getDispatcher().trigger(game::defs::HotbarUnbindCommand{target_, dragging_slot_});
    }
    clearDragState();
}

} // namespace game::ui
