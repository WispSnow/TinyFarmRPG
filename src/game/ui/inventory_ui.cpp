#include "inventory_ui.h"

#include "engine/core/context.h"
#include "engine/ui/rmlui/rml_ui_layer.h"
#include "game/component/inventory_component.h"
#include "game/defs/commands.h"
#include "game/defs/events.h"
#include "game/ui/hotbar_ui.h"
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
#include <string_view>

namespace {

constexpr int VISIBLE_SLOT_COUNT = game::component::InventoryComponent::SLOTS_PER_PAGE;
constexpr int TOTAL_SLOT_COUNT = game::component::InventoryComponent::TOTAL_SLOTS;
constexpr int TOTAL_PAGE_COUNT = game::component::InventoryComponent::PAGE_COUNT;
constexpr std::string_view DOCUMENT_PATH = "ui/rmlui/hud/inventory.rml";
constexpr std::string_view MODEL_NAME = "inventory_ui";

[[nodiscard]] int getSlotArgument(const Rml::VariantList& arguments) {
    return (arguments.size() == 1 ? arguments[0].Get<int>(-1) : -1);
}

} // namespace

namespace game::ui {

InventoryUI::InventoryUI(engine::ui::rmlui::RmlUILayer& layer,
                         engine::core::Context& context,
                         uint64_t owner_scene_id,
                         game::data::ItemCatalog* catalog)
    : layer_(layer),
      context_(context),
      item_catalog_(catalog),
      owner_scene_id_(owner_scene_id) {
    inventory_slots_.resize(VISIBLE_SLOT_COUNT);
    slot_items_.resize(TOTAL_SLOT_COUNT);
    updatePageText();
    refreshAllSlotViewModels();

    context_.getDispatcher().sink<game::defs::InventoryChanged>().connect<&InventoryUI::onInventoryChanged>(this);
    if (!initDocument()) {
        spdlog::error("InventoryUI: 初始化失败。");
    }
}

InventoryUI::~InventoryUI() {
    context_.getDispatcher().sink<game::defs::InventoryChanged>().disconnect<&InventoryUI::onInventoryChanged>(this);
    clearTooltip();
    destroyDocument();
    data_bridge_.destroy();
}

bool InventoryUI::initDocument() {
    auto* rml_context = layer_.getContext();
    if (!rml_context) {
        spdlog::error("InventoryUI: RmlUi context 不可用。");
        return false;
    }

    auto constructor = data_bridge_.create(rml_context, MODEL_NAME, &type_register_);
    if (!constructor) {
        spdlog::error("InventoryUI: 创建 data model 失败。");
        return false;
    }

    if (!ensureDataTypesRegistered(constructor)) {
        spdlog::error("InventoryUI: 注册 inventory data types 失败。");
        data_bridge_.destroy();
        return false;
    }

    if (!constructor.Bind("inventory_slots", &inventory_slots_) ||
        !constructor.Bind("page_text", &page_text_)) {
        spdlog::error("InventoryUI: 绑定 data model 字段失败。");
        data_bridge_.destroy();
        return false;
    }

    if (!constructor.BindEventCallback("on_close",
            [this](Rml::DataModelHandle, Rml::Event& event, const Rml::VariantList&) { onClose(event); }) ||
        !constructor.BindEventCallback("on_page_left",
            [this](Rml::DataModelHandle, Rml::Event& event, const Rml::VariantList&) { onPageLeft(event); }) ||
        !constructor.BindEventCallback("on_page_right",
            [this](Rml::DataModelHandle, Rml::Event& event, const Rml::VariantList&) { onPageRight(event); })) {
        spdlog::error("InventoryUI: 绑定静态 data event 回调失败。");
        data_bridge_.destroy();
        return false;
    }

    const auto bind_slot_event =
        [this, &constructor](const char* name, void (InventoryUI::*handler)(int, Rml::Event&)) {
            return constructor.BindEventCallback(name,
                [this, handler](Rml::DataModelHandle, Rml::Event& event, const Rml::VariantList& arguments) {
                    (this->*handler)(getSlotArgument(arguments), event);
                });
        };

    if (!bind_slot_event("slot_mouse_down", &InventoryUI::onSlotMouseDown) ||
        !bind_slot_event("slot_mouse_up", &InventoryUI::onSlotMouseUp) ||
        !bind_slot_event("slot_hover_enter", &InventoryUI::onSlotHoverEnter) ||
        !bind_slot_event("slot_hover_exit", &InventoryUI::onSlotHoverExit) ||
        !bind_slot_event("slot_drag_start", &InventoryUI::onSlotDragStart) ||
        !bind_slot_event("slot_drag_drop", &InventoryUI::onSlotDragDrop) ||
        !bind_slot_event("slot_drag_end", &InventoryUI::onSlotDragEnd)) {
        spdlog::error("InventoryUI: 绑定槽位 data event 回调失败。");
        data_bridge_.destroy();
        return false;
    }

    document_ = layer_.loadDocument(DOCUMENT_PATH, owner_scene_id_);
    if (!document_) {
        spdlog::error("InventoryUI: 加载 RML 文档失败: {}", DOCUMENT_PATH);
        data_bridge_.destroy();
        return false;
    }

    data_bridge_.markAllDirty();
    if (!visible_) {
        layer_.hideDocument(document_);
    }
    return true;
}

bool InventoryUI::ensureDataTypesRegistered(Rml::DataModelConstructor& constructor) {
    if (data_types_registered_) {
        return true;
    }

    if (auto slot_handle = constructor.RegisterStruct<InventorySlotViewModel>()) {
        slot_handle.RegisterMember("local_slot_index", &InventorySlotViewModel::local_slot_index);
        slot_handle.RegisterMember("inventory_index", &InventorySlotViewModel::inventory_index);
        slot_handle.RegisterMember("icon_decorator", &InventorySlotViewModel::icon_decorator);
        slot_handle.RegisterMember("count_text", &InventorySlotViewModel::count_text);
        slot_handle.RegisterMember("has_item", &InventorySlotViewModel::has_item);
        slot_handle.RegisterMember("has_count", &InventorySlotViewModel::has_count);
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

void InventoryUI::destroyDocument() {
    clearDragState();
    if (document_) {
        layer_.unloadDocument(document_);
        document_ = nullptr;
    }
}

bool InventoryUI::isValidInventoryIndex(int inventory_index) {
    return inventory_index >= 0 && inventory_index < TOTAL_SLOT_COUNT;
}

bool InventoryUI::isValidVisibleSlotIndex(int local_slot_index) {
    return local_slot_index >= 0 && local_slot_index < VISIBLE_SLOT_COUNT;
}

void InventoryUI::updatePageText() {
    const int clamped_page = std::clamp(current_page_, 0, TOTAL_PAGE_COUNT - 1);
    page_text_ = std::to_string(clamped_page + 1) + "/" + std::to_string(TOTAL_PAGE_COUNT);
}

void InventoryUI::refreshAllSlotViewModels() {
    for (int local_slot_index = 0; local_slot_index < VISIBLE_SLOT_COUNT; ++local_slot_index) {
        refreshSlotViewModel(local_slot_index);
    }
}

void InventoryUI::refreshSlotViewModel(int local_slot_index) {
    if (!isValidVisibleSlotIndex(local_slot_index)) {
        return;
    }

    auto& slot = inventory_slots_[static_cast<std::size_t>(local_slot_index)];
    const int inventory_index = current_page_ * VISIBLE_SLOT_COUNT + local_slot_index;
    slot.local_slot_index = local_slot_index;
    slot.inventory_index = inventory_index;
    slot.icon_decorator.clear();
    slot.count_text.clear();
    slot.has_item = false;
    slot.has_count = false;
    slot.can_drag = false;

    if (!isValidInventoryIndex(inventory_index)) {
        return;
    }

    const auto& item = slot_items_[static_cast<std::size_t>(inventory_index)];
    if (!item || item->item_id == entt::null || item->count <= 0) {
        return;
    }

    slot.icon_decorator = buildItemIconDecorator(item_catalog_, item->item_id);
    slot.has_item = !slot.icon_decorator.empty();
    slot.can_drag = slot.has_item;
    if (item->count > 1 && slot.has_item) {
        slot.count_text = std::to_string(item->count);
        slot.has_count = true;
    }
}

void InventoryUI::markSlotsDirty() {
    if (data_bridge_.isValid()) {
        data_bridge_.markDirty("inventory_slots");
    }
    refreshTooltipForHoveredSlot();
}

void InventoryUI::markPageDirty() {
    if (data_bridge_.isValid()) {
        data_bridge_.markDirty("page_text");
    }
}

std::optional<engine::ui::SlotItem> InventoryUI::getSlotItemData(int inventory_index) const {
    if (!isValidInventoryIndex(inventory_index)) {
        return std::nullopt;
    }
    return slot_items_[static_cast<std::size_t>(inventory_index)];
}

void InventoryUI::showTooltipForSlot(int inventory_index) {
    if (!tooltip_ui_ || !item_catalog_ || !visible_ || dragging_ || (hotbar_ui_ && hotbar_ui_->isDragging())) {
        return;
    }

    const auto slot_item = getSlotItemData(inventory_index);
    if (!slot_item || slot_item->item_id == entt::null || slot_item->count <= 0) {
        clearTooltip();
        return;
    }

    const auto* item = item_catalog_->findItem(slot_item->item_id);
    if (!item) {
        clearTooltip();
        return;
    }

    tooltip_ui_->showItem(item->display_name_, item->category_str_, item->description_);
}

void InventoryUI::refreshTooltipForHoveredSlot() {
    if (hovered_inventory_index_ < 0) {
        return;
    }
    showTooltipForSlot(hovered_inventory_index_);
}

void InventoryUI::clearTooltip() {
    hovered_inventory_index_ = -1;
    if (tooltip_ui_) {
        tooltip_ui_->hideTooltip();
    }
}

void InventoryUI::clearDragState() {
    dragging_ = false;
    drop_handled_ = false;
    dragging_inventory_index_ = -1;
}

void InventoryUI::setSlotItem(int index, const engine::ui::SlotItem& item) {
    if (!isValidInventoryIndex(index)) {
        return;
    }

    slot_items_[static_cast<std::size_t>(index)] = item;
    if (index / VISIBLE_SLOT_COUNT == current_page_) {
        refreshSlotViewModel(index % VISIBLE_SLOT_COUNT);
    }
    markSlotsDirty();
}

void InventoryUI::clearSlot(int index) {
    if (!isValidInventoryIndex(index)) {
        return;
    }

    slot_items_[static_cast<std::size_t>(index)].reset();
    if (index / VISIBLE_SLOT_COUNT == current_page_) {
        refreshSlotViewModel(index % VISIBLE_SLOT_COUNT);
    }
    markSlotsDirty();
}

void InventoryUI::clearAllSlots() {
    for (auto& item : slot_items_) {
        item.reset();
    }
    refreshAllSlotViewModels();
    markSlotsDirty();
}

void InventoryUI::show() {
    visible_ = true;
    if (document_) {
        layer_.showDocument(document_);
    }
}

void InventoryUI::hide() {
    visible_ = false;
    clearTooltip();
    clearDragState();
    if (document_) {
        layer_.hideDocument(document_);
    }
}

void InventoryUI::toggle() {
    if (visible_) {
        hide();
    } else {
        show();
    }
}

void InventoryUI::changePage(int delta) {
    const int new_page = std::clamp(current_page_ + delta, 0, TOTAL_PAGE_COUNT - 1);
    if (new_page == current_page_) {
        return;
    }

    clearTooltip();
    clearDragState();
    current_page_ = new_page;
    updatePageText();
    refreshAllSlotViewModels();
    markPageDirty();
    markSlotsDirty();

    if (target_ != entt::null) {
        context_.getDispatcher().trigger(game::defs::InventorySetActivePageCommand{target_, current_page_});
    }
}

void InventoryUI::onInventoryChanged(const game::defs::InventoryChanged& evt) {
    if (target_ == entt::null || evt.target != target_) {
        return;
    }

    if (evt.full_sync) {
        for (auto& item : slot_items_) {
            item.reset();
        }
    }

    for (const auto& slot : evt.slots) {
        if (!isValidInventoryIndex(slot.slot_index)) {
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

    const int new_page = std::clamp(evt.active_page, 0, TOTAL_PAGE_COUNT - 1);
    if (new_page != current_page_) {
        clearTooltip();
        clearDragState();
        current_page_ = new_page;
    }

    updatePageText();
    refreshAllSlotViewModels();
    markPageDirty();
    markSlotsDirty();
}

void InventoryUI::onClose(Rml::Event& event) {
    event.StopPropagation();
    hide();
}

void InventoryUI::onPageLeft(Rml::Event& event) {
    event.StopPropagation();
    changePage(-1);
}

void InventoryUI::onPageRight(Rml::Event& event) {
    event.StopPropagation();
    changePage(1);
}

void InventoryUI::onSlotMouseDown(int inventory_index, Rml::Event& event) {
    if (!isValidInventoryIndex(inventory_index)) {
        return;
    }
    event.StopPropagation();
}

void InventoryUI::onSlotMouseUp(int inventory_index, Rml::Event& event) {
    if (!isValidInventoryIndex(inventory_index)) {
        return;
    }

    const int button = event.GetParameter("button", -1);
    if (button < 0) {
        return;
    }

    event.StopPropagation();
    if (button != 2 || target_ == entt::null) {
        return;
    }

    const auto slot_item = getSlotItemData(inventory_index);
    if (!slot_item || slot_item->item_id == entt::null || slot_item->count <= 0) {
        return;
    }

    bool can_use = false;
    if (item_catalog_) {
        if (const auto* item = item_catalog_->findItem(slot_item->item_id)) {
            can_use = item->on_use_.has_value();
        }
    }

    if (can_use) {
        context_.getDispatcher().trigger(game::defs::UseItemCommand{target_, inventory_index, 1, true});
    }
}

void InventoryUI::onSlotHoverEnter(int inventory_index, Rml::Event& event) {
    if (!isValidInventoryIndex(inventory_index)) {
        return;
    }
    event.StopPropagation();
    hovered_inventory_index_ = inventory_index;
    showTooltipForSlot(inventory_index);
}

void InventoryUI::onSlotHoverExit(int inventory_index, Rml::Event& event) {
    if (!isValidInventoryIndex(inventory_index)) {
        return;
    }
    event.StopPropagation();
    if (hovered_inventory_index_ == inventory_index) {
        clearTooltip();
    }
}

void InventoryUI::onSlotDragStart(int inventory_index, Rml::Event& event) {
    if (!isValidInventoryIndex(inventory_index) || target_ == entt::null) {
        return;
    }

    const auto slot_item = getSlotItemData(inventory_index);
    if (!slot_item || slot_item->item_id == entt::null || slot_item->count <= 0) {
        return;
    }

    event.StopPropagation();
    clearTooltip();
    dragging_ = true;
    drop_handled_ = false;
    dragging_inventory_index_ = inventory_index;
}

void InventoryUI::onSlotDragDrop(int inventory_index, Rml::Event& event) {
    if (!isValidInventoryIndex(inventory_index) || target_ == entt::null) {
        return;
    }

    if (dragging_) {
        event.StopPropagation();
        clearTooltip();
        drop_handled_ = true;
        if (inventory_index != dragging_inventory_index_) {
            context_.getDispatcher().trigger(
                game::defs::InventoryMoveCommand{target_, dragging_inventory_index_, inventory_index, true});
        }
        return;
    }

    if (!hotbar_ui_ || !hotbar_ui_->isDragging()) {
        return;
    }

    const int source_inventory_index = hotbar_ui_->getDragInventoryIndex();
    if (!isValidInventoryIndex(source_inventory_index)) {
        return;
    }

    event.StopPropagation();
    clearTooltip();
    if (source_inventory_index != inventory_index) {
        context_.getDispatcher().trigger(
            game::defs::InventoryMoveCommand{target_, source_inventory_index, inventory_index, true});
    }
    hotbar_ui_->notifyExternalDropHandled();
}

void InventoryUI::onSlotDragEnd(int inventory_index, Rml::Event& event) {
    if (!dragging_ || inventory_index != dragging_inventory_index_) {
        return;
    }

    event.StopPropagation();
    clearDragState();
}

} // namespace game::ui
