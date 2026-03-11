#include "inventory_menu_ui.h"

#include "engine/core/context.h"
#include "engine/core/game_state.h"
#include "engine/ui/rmlui/hover_focus_sync_listener.h"
#include "engine/ui/rmlui/rml_bind_helpers.h"
#include "engine/ui/rmlui/rml_element_helpers.h"
#include "engine/ui/rmlui/rml_ui_layer.h"
#include "game/component/inventory_component.h"
#include "game/defs/commands.h"
#include "game/defs/events.h"
#include "game/ui/item_tooltip_ui.h"
#include "game/ui/rml_item_icon_helpers.h"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/Types.h>
#include <entt/signal/dispatcher.hpp>
#include <glm/vec2.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>

namespace {

constexpr int TOTAL_SLOT_COUNT = game::component::InventoryComponent::TOTAL_SLOTS;
constexpr float ACTION_MENU_FALLBACK_WIDTH = 104.0f;
constexpr float ACTION_MENU_FALLBACK_HEIGHT = 92.0f;
constexpr float ACTION_MENU_OFFSET_X = 8.0f;

constexpr std::string_view DOCUMENT_PATH = "ui/rmlui/scenes/inventory_menu.rml";
constexpr std::string_view MODEL_NAME = "inventory_menu";
constexpr std::string_view kFocusableClass = "inventory-menu-focusable";
constexpr std::string_view kSlotClass = "inventory-menu-slot-button";
constexpr std::string_view kActionButtonClass = "inventory-menu-action-button";

using engine::ui::rmlui::updateBoundBool;
using engine::ui::rmlui::updateBoundString;

[[nodiscard]] int getSlotArgument(const Rml::VariantList& arguments) {
    return (arguments.size() == 1 ? arguments[0].Get<int>(-1) : -1);
}

[[nodiscard]] Rml::String toDpString(float value) {
    return std::to_string(static_cast<int>(std::lround(value))) + "dp";
}

} // namespace

namespace game::ui {

InventoryMenuUI::InventoryMenuUI(engine::ui::rmlui::RmlUILayer& layer,
                                 engine::core::Context& context,
                                 uint64_t owner_scene_id,
                                 game::data::ItemCatalog* catalog)
    : layer_(layer),
      context_(context),
      item_catalog_(catalog),
      owner_scene_id_(owner_scene_id) {
    inventory_slots_.resize(TOTAL_SLOT_COUNT);
    slot_items_.resize(TOTAL_SLOT_COUNT);
    refreshAllSlotViewModels();

    context_.getDispatcher().sink<game::defs::InventoryChanged>().connect<&InventoryMenuUI::onInventoryChanged>(this);
    if (!initDocument()) {
        spdlog::error("InventoryMenuUI: 初始化失败。");
    }
}

InventoryMenuUI::~InventoryMenuUI() {
    context_.getDispatcher().sink<game::defs::InventoryChanged>().disconnect<&InventoryMenuUI::onInventoryChanged>(this);
    clearTooltip();
    destroyDocument();
    data_bridge_.destroy();
}

void InventoryMenuUI::syncFromTarget() {
    if (target_ == entt::null) {
        return;
    }

    context_.getDispatcher().trigger(game::defs::InventorySyncCommand{target_});
}

void InventoryMenuUI::queueInitialFocus() {
    focusSlot(0);
}

void InventoryMenuUI::update(float delta_time) {
    (void)delta_time;
    refreshSelectionFromFocus();
    updateActionMenuPlacement();
    refreshTooltipForSelection();
}

bool InventoryMenuUI::handleMenuCancel() {
    if (!action_menu_open_) {
        return false;
    }

    closeActionMenu(true);
    return true;
}

bool InventoryMenuUI::consumeCloseRequested() {
    const bool result = close_requested_;
    close_requested_ = false;
    return result;
}

bool InventoryMenuUI::initDocument() {
    auto* rml_context = layer_.getContext();
    if (!rml_context) {
        spdlog::error("InventoryMenuUI: RmlUi context 不可用。");
        return false;
    }

    auto constructor = data_bridge_.create(rml_context, MODEL_NAME, &type_register_);
    if (!constructor) {
        spdlog::error("InventoryMenuUI: 创建 data model 失败。");
        return false;
    }

    if (!ensureDataTypesRegistered(constructor)) {
        spdlog::error("InventoryMenuUI: 注册 data types 失败。");
        data_bridge_.destroy();
        return false;
    }

    if (!constructor.Bind("inventory_slots", &inventory_slots_) ||
        !constructor.Bind("action_menu_open", &action_menu_open_) ||
        !constructor.Bind("can_use_selected", &can_use_selected_) ||
        !constructor.Bind("can_discard_selected", &can_discard_selected_) ||
        !constructor.Bind("action_menu_left", &action_menu_left_) ||
        !constructor.Bind("action_menu_top", &action_menu_top_) ||
        !constructor.Bind("action_menu_title", &action_menu_title_) ||
        !constructor.Bind("discard_text", &discard_text_)) {
        spdlog::error("InventoryMenuUI: 绑定 data model 字段失败。");
        data_bridge_.destroy();
        return false;
    }

    const auto bind_slot_event =
        [this, &constructor](const char* name, void (InventoryMenuUI::*handler)(int, Rml::Event&)) {
            return constructor.BindEventCallback(
                name,
                [this, handler](Rml::DataModelHandle, Rml::Event& event, const Rml::VariantList& arguments) {
                    (this->*handler)(getSlotArgument(arguments), event);
                });
        };

    if (!constructor.BindEventCallback("on_close",
            [this](Rml::DataModelHandle, Rml::Event& event, const Rml::VariantList&) { onClose(event); }) ||
        !bind_slot_event("slot_confirm", &InventoryMenuUI::onSlotConfirm) ||
        !bind_slot_event("slot_drag_start", &InventoryMenuUI::onSlotDragStart) ||
        !bind_slot_event("slot_drag_drop", &InventoryMenuUI::onSlotDragDrop) ||
        !bind_slot_event("slot_drag_end", &InventoryMenuUI::onSlotDragEnd) ||
        !constructor.BindEventCallback("action_use",
            [this](Rml::DataModelHandle, Rml::Event& event, const Rml::VariantList&) { onActionUse(event); }) ||
        !constructor.BindEventCallback("action_discard",
            [this](Rml::DataModelHandle, Rml::Event& event, const Rml::VariantList&) { onActionDiscard(event); }) ||
        !constructor.BindEventCallback("action_cancel",
            [this](Rml::DataModelHandle, Rml::Event& event, const Rml::VariantList&) { onActionCancel(event); })) {
        spdlog::error("InventoryMenuUI: 绑定 data event 回调失败。");
        data_bridge_.destroy();
        return false;
    }

    document_ = layer_.loadDocument(DOCUMENT_PATH, owner_scene_id_);
    if (!document_) {
        spdlog::error("InventoryMenuUI: 加载 RML 文档失败: {}", DOCUMENT_PATH);
        data_bridge_.destroy();
        return false;
    }

    collectSlotButtons();

    hover_focus_listener_ = std::make_unique<engine::ui::rmlui::HoverFocusSyncListener>(
        layer_,
        [](Rml::Element* element) {
            return element != nullptr && element->IsClassSet(Rml::String{kFocusableClass.data(), kFocusableClass.size()});
        });
    document_->AddEventListener("mouseover", hover_focus_listener_.get());
    hover_listener_registered_ = true;

    refreshActionMenuState();
    data_bridge_.markAllDirty();
    return true;
}

bool InventoryMenuUI::ensureDataTypesRegistered(Rml::DataModelConstructor& constructor) {
    if (data_types_registered_) {
        return true;
    }

    if (auto slot_handle = constructor.RegisterStruct<InventorySlotViewModel>()) {
        slot_handle.RegisterMember("slot_index", &InventorySlotViewModel::slot_index);
        slot_handle.RegisterMember("icon_decorator", &InventorySlotViewModel::icon_decorator);
        slot_handle.RegisterMember("count_text", &InventorySlotViewModel::count_text);
        slot_handle.RegisterMember("has_item", &InventorySlotViewModel::has_item);
        slot_handle.RegisterMember("has_count", &InventorySlotViewModel::has_count);
        slot_handle.RegisterMember("is_selected", &InventorySlotViewModel::is_selected);
        slot_handle.RegisterMember("can_drag", &InventorySlotViewModel::can_drag);
    } else {
        return false;
    }

    if (!constructor.RegisterArray<decltype(inventory_slots_)>()) {
        return false;
    }

    data_types_registered_ = true;
    return true;
}

void InventoryMenuUI::destroyDocument() {
    removeEventListeners();
    clearDragState();
    slot_buttons_.clear();
    if (document_) {
        layer_.unloadDocument(document_);
        document_ = nullptr;
    }
}

void InventoryMenuUI::removeEventListeners() {
    if (document_ && hover_listener_registered_ && hover_focus_listener_) {
        document_->RemoveEventListener("mouseover", hover_focus_listener_.get());
        hover_listener_registered_ = false;
    }
}

void InventoryMenuUI::collectSlotButtons() {
    slot_buttons_.clear();
    if (!document_) {
        return;
    }

    auto* grid = document_->GetElementById("inventory-menu-grid");
    if (!grid) {
        spdlog::warn("InventoryMenuUI: 缺少 inventory-menu-grid。");
        return;
    }

    slot_buttons_.reserve(inventory_slots_.size());
    for (int child_index = 0; child_index < grid->GetNumChildren(); ++child_index) {
        auto* element = grid->GetChild(child_index);
        if (!element || element->GetTagName() != "button") {
            continue;
        }
        if (element->HasAttribute("data-for")) {
            continue;
        }
        if (!element->IsClassSet(Rml::String{kSlotClass.data(), kSlotClass.size()})) {
            continue;
        }
        slot_buttons_.push_back(element);
    }

    if (slot_buttons_.size() != inventory_slots_.size()) {
        spdlog::warn("InventoryMenuUI: slot button 数量异常，期望 {} 实际 {}。",
                     inventory_slots_.size(),
                     slot_buttons_.size());
    }
}

bool InventoryMenuUI::isValidSlotIndex(int slot_index) {
    return slot_index >= 0 && slot_index < TOTAL_SLOT_COUNT;
}

void InventoryMenuUI::refreshAllSlotViewModels() {
    for (int slot_index = 0; slot_index < TOTAL_SLOT_COUNT; ++slot_index) {
        refreshSlotViewModel(slot_index);
    }
}

void InventoryMenuUI::refreshSlotViewModel(int slot_index) {
    if (!isValidSlotIndex(slot_index)) {
        return;
    }

    auto& slot = inventory_slots_[static_cast<std::size_t>(slot_index)];
    slot.slot_index = slot_index;
    slot.icon_decorator = std::string{game::ui::kNoDecorator};
    slot.count_text.clear();
    slot.has_item = false;
    slot.has_count = false;
    slot.is_selected = (slot_index == selected_slot_index_);
    slot.can_drag = false;

    const auto& item = slot_items_[static_cast<std::size_t>(slot_index)];
    if (!item || item->item_id == entt::null || item->count <= 0) {
        return;
    }

    slot.icon_decorator = buildItemIconDecorator(item_catalog_, item->item_id);
    slot.has_item = hasDecorator(slot.icon_decorator);
    slot.can_drag = slot.has_item;
    if (item->count > 1 && slot.has_item) {
        slot.count_text = std::to_string(item->count);
        slot.has_count = true;
    }
}

void InventoryMenuUI::markSlotsDirty() {
    if (data_bridge_.isValid()) {
        data_bridge_.markDirty("inventory_slots");
    }
}

void InventoryMenuUI::clearDragState() {
    dragging_ = false;
    drop_handled_ = false;
    suppress_next_confirm_ = false;
    dragging_slot_index_ = -1;
}

void InventoryMenuUI::refreshSelectionFromFocus() {
    if (action_menu_open_) {
        return;
    }

    const int new_selected_slot = findSlotIndexForElement(layer_.getFocusedElement());
    if (new_selected_slot == selected_slot_index_) {
        return;
    }

    const int previous_selected_slot = selected_slot_index_;
    selected_slot_index_ = new_selected_slot;

    if (isValidSlotIndex(previous_selected_slot)) {
        refreshSlotViewModel(previous_selected_slot);
    }
    if (isValidSlotIndex(selected_slot_index_)) {
        refreshSlotViewModel(selected_slot_index_);
    }

    markSlotsDirty();
}

int InventoryMenuUI::findSlotIndexForElement(Rml::Element* element) const {
    while (element != nullptr) {
        const auto it = std::find(slot_buttons_.begin(), slot_buttons_.end(), element);
        if (it != slot_buttons_.end()) {
            return static_cast<int>(std::distance(slot_buttons_.begin(), it));
        }
        element = element->GetParentNode();
    }

    return -1;
}

Rml::Element* InventoryMenuUI::slotButton(int slot_index) const {
    if (!isValidSlotIndex(slot_index)) {
        return nullptr;
    }

    const std::size_t idx = static_cast<std::size_t>(slot_index);
    if (idx >= slot_buttons_.size()) {
        return nullptr;
    }

    return slot_buttons_[idx];
}

void InventoryMenuUI::focusSlot(int slot_index) {
    if (auto* element = slotButton(slot_index)) {
        layer_.queueFocusElement(element);
    }
}

std::optional<engine::ui::SlotItem> InventoryMenuUI::getSlotItemData(int slot_index) const {
    if (!isValidSlotIndex(slot_index)) {
        return std::nullopt;
    }

    return slot_items_[static_cast<std::size_t>(slot_index)];
}

const game::data::ItemData* InventoryMenuUI::getSlotItemMeta(int slot_index) const {
    const auto item = getSlotItemData(slot_index);
    if (!item || item->item_id == entt::null || item->count <= 0 || !item_catalog_) {
        return nullptr;
    }

    return item_catalog_->findItem(item->item_id);
}

void InventoryMenuUI::refreshTooltipForSelection() {
    if (!tooltip_ui_ || action_menu_open_ || dragging_) {
        clearTooltip();
        return;
    }

    const auto slot_item = getSlotItemData(selected_slot_index_);
    const auto* item = getSlotItemMeta(selected_slot_index_);
    auto* button = slotButton(selected_slot_index_);
    if (!slot_item || !item || !button || slot_item->item_id == entt::null || slot_item->count <= 0) {
        clearTooltip();
        return;
    }

    const Rml::Vector2f slot_pos = button->GetAbsoluteOffset(Rml::BoxArea::Border);
    const glm::vec2 anchor_pos{slot_pos.x, slot_pos.y};
    const glm::vec2 anchor_size{button->GetOffsetWidth(), button->GetOffsetHeight()};
    tooltip_ui_->setAnchorRect(anchor_pos, anchor_size);

    if (tooltip_slot_index_ != selected_slot_index_ || tooltip_item_id_ != slot_item->item_id) {
        tooltip_ui_->showItem(item->display_name_, item->category_str_, item->description_);
        tooltip_slot_index_ = selected_slot_index_;
        tooltip_item_id_ = slot_item->item_id;
    }
}

void InventoryMenuUI::clearTooltip() {
    tooltip_slot_index_ = -1;
    tooltip_item_id_ = entt::null;
    if (tooltip_ui_) {
        tooltip_ui_->hideTooltip();
    }
}

void InventoryMenuUI::openActionMenuForSlot(int slot_index) {
    if (!isValidSlotIndex(slot_index) || !getSlotItemData(slot_index).has_value()) {
        return;
    }

    if (selected_slot_index_ != slot_index) {
        const int previous_selected_slot = selected_slot_index_;
        selected_slot_index_ = slot_index;
        if (isValidSlotIndex(previous_selected_slot)) {
            refreshSlotViewModel(previous_selected_slot);
        }
        refreshSlotViewModel(selected_slot_index_);
        markSlotsDirty();
    }

    action_menu_open_ = true;
    action_menu_target_slot_ = slot_index;
    discard_confirm_ = false;
    refreshActionMenuState();
    updateActionMenuPlacement();
    clearTooltip();
    layer_.queueFocusFirstEnabledElementByClass(document_, kActionButtonClass);
}

void InventoryMenuUI::closeActionMenu(bool restore_focus) {
    action_menu_open_ = false;
    action_menu_target_slot_ = -1;
    discard_confirm_ = false;
    refreshActionMenuState();
    if (restore_focus) {
        focusSlot(selected_slot_index_);
    }
}

void InventoryMenuUI::refreshActionMenuState() {
    const auto* item = getSlotItemMeta(action_menu_target_slot_);
    const bool can_use = (item != nullptr && item->on_use_.has_value());
    const bool can_discard = getSlotItemData(action_menu_target_slot_).has_value();

    if (updateBoundBool(can_use_selected_, can_use)) {
        data_bridge_.markDirty("can_use_selected");
    }
    if (updateBoundBool(can_discard_selected_, can_discard)) {
        data_bridge_.markDirty("can_discard_selected");
    }
    if (data_bridge_.isValid()) {
        data_bridge_.markDirty("action_menu_open");
    }

    const std::string title = discard_confirm_
        ? std::string{"Discard 1 item?"}
        : (item ? item->display_name_ : std::string{"Item"});
    if (updateBoundString(action_menu_title_, title)) {
        data_bridge_.markDirty("action_menu_title");
    }

    const std::string discard_text = discard_confirm_ ? "Confirm" : "Discard";
    if (updateBoundString(discard_text_, discard_text)) {
        data_bridge_.markDirty("discard_text");
    }
}

void InventoryMenuUI::updateActionMenuPlacement() {
    if (!action_menu_open_) {
        return;
    }

    auto* button = slotButton(action_menu_target_slot_);
    if (!button) {
        return;
    }

    const glm::vec2 logical_size = context_.getGameState().getLogicalSize();
    const Rml::Vector2f slot_pos = button->GetAbsoluteOffset(Rml::BoxArea::Border);
    const glm::vec2 anchor_pos{slot_pos.x, slot_pos.y};
    const glm::vec2 anchor_size{button->GetOffsetWidth(), button->GetOffsetHeight()};

    float menu_width = ACTION_MENU_FALLBACK_WIDTH;
    float menu_height = ACTION_MENU_FALLBACK_HEIGHT;
    if (auto* action_menu = document_ ? document_->GetElementById("inventory-menu-action-menu") : nullptr) {
        menu_width = std::max(menu_width, action_menu->GetOffsetWidth());
        menu_height = std::max(menu_height, action_menu->GetOffsetHeight());
    }

    glm::vec2 pos = anchor_pos + glm::vec2{anchor_size.x + ACTION_MENU_OFFSET_X, -4.0f};
    if (pos.x + menu_width > logical_size.x) {
        pos.x = anchor_pos.x - ACTION_MENU_OFFSET_X - menu_width;
    }
    if (pos.y + menu_height > logical_size.y) {
        pos.y = logical_size.y - menu_height;
    }

    pos.x = std::clamp(pos.x, 0.0f, std::max(0.0f, logical_size.x - menu_width));
    pos.y = std::clamp(pos.y, 0.0f, std::max(0.0f, logical_size.y - menu_height));

    if (updateBoundString(action_menu_left_, toDpString(pos.x))) {
        data_bridge_.markDirty("action_menu_left");
    }
    if (updateBoundString(action_menu_top_, toDpString(pos.y))) {
        data_bridge_.markDirty("action_menu_top");
    }
}

void InventoryMenuUI::onInventoryChanged(const game::defs::InventoryChanged& evt) {
    if (target_ == entt::null || evt.target != target_) {
        return;
    }

    if (evt.full_sync) {
        for (auto& item : slot_items_) {
            item.reset();
        }
    }

    for (const auto& slot : evt.slots) {
        if (!isValidSlotIndex(slot.slot_index)) {
            continue;
        }

        auto& cached = slot_items_[static_cast<std::size_t>(slot.slot_index)];
        if (slot.item_id != entt::null && slot.count > 0) {
            cached = engine::ui::SlotItem{
                slot.item_id,
                slot.count,
                item_catalog_ ? item_catalog_->getItemIcon(slot.item_id) : engine::render::Image{}};
        } else {
            cached.reset();
        }
    }

    refreshAllSlotViewModels();
    markSlotsDirty();

    if (action_menu_open_) {
        const auto slot_item = getSlotItemData(action_menu_target_slot_);
        if (!slot_item || slot_item->item_id == entt::null || slot_item->count <= 0) {
            closeActionMenu(true);
        } else {
            refreshActionMenuState();
        }
    }
}

void InventoryMenuUI::onClose(Rml::Event& event) {
    event.StopPropagation();
    close_requested_ = true;
}

void InventoryMenuUI::onSlotConfirm(int slot_index, Rml::Event& event) {
    event.StopPropagation();
    if (suppress_next_confirm_) {
        suppress_next_confirm_ = false;
        return;
    }

    const auto slot_item = getSlotItemData(slot_index);
    if (!slot_item || slot_item->item_id == entt::null || slot_item->count <= 0) {
        return;
    }

    openActionMenuForSlot(slot_index);
}

void InventoryMenuUI::onSlotDragStart(int slot_index, Rml::Event& event) {
    if (!isValidSlotIndex(slot_index) || target_ == entt::null || action_menu_open_) {
        return;
    }

    const auto slot_item = getSlotItemData(slot_index);
    if (!slot_item || slot_item->item_id == entt::null || slot_item->count <= 0) {
        return;
    }

    event.StopPropagation();
    clearTooltip();
    dragging_ = true;
    drop_handled_ = false;
    suppress_next_confirm_ = true;
    dragging_slot_index_ = slot_index;
}

void InventoryMenuUI::onSlotDragDrop(int slot_index, Rml::Event& event) {
    if (!dragging_ || !isValidSlotIndex(slot_index) || !isValidSlotIndex(dragging_slot_index_) || target_ == entt::null) {
        return;
    }

    event.StopPropagation();
    clearTooltip();
    drop_handled_ = true;
    focusSlot(slot_index);

    if (slot_index != dragging_slot_index_) {
        context_.getDispatcher().trigger(
            game::defs::InventoryMoveCommand{target_, dragging_slot_index_, slot_index, true});
    }
}

void InventoryMenuUI::onSlotDragEnd(int slot_index, Rml::Event& event) {
    if (!dragging_ || slot_index != dragging_slot_index_) {
        return;
    }

    event.StopPropagation();
    clearDragState();
}

void InventoryMenuUI::onActionUse(Rml::Event& event) {
    event.StopPropagation();
    if (!action_menu_open_ || target_ == entt::null || !can_use_selected_) {
        return;
    }

    const int slot_index = action_menu_target_slot_;
    closeActionMenu(true);
    context_.getDispatcher().trigger(game::defs::UseItemCommand{target_, slot_index, 1, true});
}

void InventoryMenuUI::onActionDiscard(Rml::Event& event) {
    event.StopPropagation();
    if (!action_menu_open_ || target_ == entt::null || !can_discard_selected_) {
        return;
    }

    const auto slot_item = getSlotItemData(action_menu_target_slot_);
    if (!slot_item || slot_item->item_id == entt::null || slot_item->count <= 0) {
        return;
    }

    if (!discard_confirm_) {
        discard_confirm_ = true;
        refreshActionMenuState();
        return;
    }

    const int slot_index = action_menu_target_slot_;
    const entt::id_type item_id = slot_item->item_id;
    closeActionMenu(true);
    context_.getDispatcher().trigger(game::defs::RemoveItemCommand{target_, item_id, 1, slot_index});
}

void InventoryMenuUI::onActionCancel(Rml::Event& event) {
    event.StopPropagation();
    closeActionMenu(true);
}

} // namespace game::ui
