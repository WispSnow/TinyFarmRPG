#include "inventory_menu_scene.h"

#include "engine/component/name_component.h"
#include "engine/core/context.h"
#include "engine/core/game_state.h"
#include "engine/input/input_manager.h"
#include "engine/ui/rmlui/rml_mouse_buttons.h"
#include "engine/ui/rmlui/rml_ui_runtime.h"
#include "game/component/hotbar_component.h"
#include "game/component/inventory_component.h"
#include "game/data/item_catalog.h"
#include "game/defs/commands.h"
#include "game/defs/events.h"
#include "game/ui/item_tooltip_ui.h"
#include "game/ui/rml_item_icon_helpers.h"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/DataTypeRegister.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Event.h>
#include <entt/core/hashed_string.hpp>
#include <entt/signal/dispatcher.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>

using namespace entt::literals;

namespace {

constexpr int TOTAL_SLOTS = game::component::InventoryComponent::TOTAL_SLOTS;
constexpr int HOTBAR_SLOTS = game::component::HotbarComponent::SLOT_COUNT;
constexpr std::string_view DOCUMENT_PATH = "ui/rmlui/scenes/inventory_menu.rml";
constexpr std::string_view MODEL_NAME = "inventory_menu";

constexpr float SLOT_SIZE_DP = 20.0F;
constexpr float SLOT_GAP_DP = 2.0F;
constexpr float HOTBAR_TOP_DP = 0.0F;
constexpr float BACKPACK_TOP_DP = 24.0F;
constexpr float ACTION_MENU_GAP_DP = 6.0F;
constexpr float INVENTORY_COLUMN_WIDTH_DP = 218.0F;
constexpr float ACTION_MENU_WIDTH_DP = 84.0F;
constexpr float ACTION_MENU_HEADER_DP = 18.0F;
constexpr float ACTION_MENU_ENTRY_DP = 18.0F;
constexpr float ACTION_MENU_MAX_TOP_DP = 138.0F;

enum class MenuActionId : int {
    Use = 1,
    Discard,
    Activate,
    Unbind,
    ConfirmDiscard,
    Cancel
};

[[nodiscard]] int getSlotArgument(const Rml::VariantList& arguments) {
    return (arguments.size() == 1 ? arguments[0].Get<int>(-1) : -1);
}

[[nodiscard]] Rml::String toDpString(float value) {
    return std::to_string(static_cast<int>(std::round(value))) + "dp";
}

[[nodiscard]] const game::component::InventoryComponent*
tryGetInventory(const entt::registry& registry, entt::entity player) {
    return registry.try_get<game::component::InventoryComponent>(player);
}

[[nodiscard]] const game::component::HotbarComponent*
tryGetHotbar(const entt::registry& registry, entt::entity player) {
    return registry.try_get<game::component::HotbarComponent>(player);
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
    return item && item->on_use_.has_value();
}

} // namespace

namespace game::scene {

InventoryMenuScene::InventoryMenuScene(std::string_view name,
                                       engine::core::Context& context,
                                       entt::registry& game_registry,
                                       entt::entity player,
                                       game::data::ItemCatalog* item_catalog)
    : engine::scene::Scene(name, context),
      game_registry_(game_registry),
      player_(player),
      item_catalog_(item_catalog),
      previous_state_(context.getGameState().getCurrentState()) {
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

InventoryMenuScene::~InventoryMenuScene() {
    disconnectRuntimeListeners();
    focus_before_action_menu_ = nullptr;
    action_menu_entries_.clear();
    action_menu_visible_ = false;
    tooltip_ui_.reset();
    unloadOwnedRmlDocumentsNow();
}

bool InventoryMenuScene::init() {
    previous_state_ = context_.getGameState().getCurrentState();
    context_.getGameState().setState(engine::core::State::Paused);
    context_.getInputManager().pushContext(engine::input::InputContextId::Menu);
    context_pushed_ = true;

    if (!initUI()) {
        return false;
    }

    context_.getInputManager().onAction("menu_cancel"_hs)
        .connect<&InventoryMenuScene::onMenuCancelPressed>(this);
    context_.getDispatcher().sink<game::defs::InventoryChanged>()
        .connect<&InventoryMenuScene::onInventoryChanged>(this);
    context_.getDispatcher().sink<game::defs::HotbarChanged>()
        .connect<&InventoryMenuScene::onHotbarChanged>(this);

    return Scene::init();
}

void InventoryMenuScene::update(float delta_time) {
    if (tooltip_ui_) {
        tooltip_ui_->update(delta_time);
    }
    Scene::update(delta_time);
}

void InventoryMenuScene::clean() {
    disconnectRuntimeListeners();
    clearTooltip();
    closeActionMenu(false);
    tooltip_ui_.reset();
    context_.getGameState().setState(previous_state_);
    if (context_pushed_) {
        context_.getInputManager().popContext();
        context_pushed_ = false;
    }
    Scene::clean();
}

bool InventoryMenuScene::initUI() {
    auto* runtime = context_.getRmlUi();
    if (!runtime) {
        spdlog::error("InventoryMenuScene: RmlUiRuntime 不可用。");
        return false;
    }

    auto* rml_context = runtime->getContext();
    if (!rml_context) {
        spdlog::error("InventoryMenuScene: RmlUi context 不可用。");
        return false;
    }

    auto constructor = data_bridge_.create(rml_context, MODEL_NAME, &type_register_);
    if (!constructor) {
        spdlog::error("InventoryMenuScene: 创建 data model 失败。");
        return false;
    }

    if (!data_types_registered_) {
        if (auto slot_handle = constructor.RegisterStruct<SlotViewModel>()) {
            slot_handle.RegisterMember("slot_index", &SlotViewModel::slot_index);
            slot_handle.RegisterMember("icon_decorator", &SlotViewModel::icon_decorator);
            slot_handle.RegisterMember("count_text", &SlotViewModel::count_text);
            slot_handle.RegisterMember("has_item", &SlotViewModel::has_item);
            slot_handle.RegisterMember("has_count", &SlotViewModel::has_count);
            slot_handle.RegisterMember("can_drag", &SlotViewModel::can_drag);
            slot_handle.RegisterMember("is_selected", &SlotViewModel::is_selected);
        } else {
            spdlog::error("InventoryMenuScene: RegisterStruct<SlotViewModel> 失败。");
            data_bridge_.destroy();
            return false;
        }

        if (!constructor.RegisterArray<decltype(backpack_slots_)>()) {
            spdlog::error("InventoryMenuScene: RegisterArray<backpack> 失败。");
            data_bridge_.destroy();
            return false;
        }

        if (auto hotbar_handle = constructor.RegisterStruct<HotbarSlotViewModel>()) {
            hotbar_handle.RegisterMember("slot_index", &HotbarSlotViewModel::slot_index);
            hotbar_handle.RegisterMember("icon_decorator", &HotbarSlotViewModel::icon_decorator);
            hotbar_handle.RegisterMember("count_text", &HotbarSlotViewModel::count_text);
            hotbar_handle.RegisterMember("label", &HotbarSlotViewModel::label);
            hotbar_handle.RegisterMember("has_item", &HotbarSlotViewModel::has_item);
            hotbar_handle.RegisterMember("has_count", &HotbarSlotViewModel::has_count);
            hotbar_handle.RegisterMember("is_active", &HotbarSlotViewModel::is_active);
            hotbar_handle.RegisterMember("can_drag", &HotbarSlotViewModel::can_drag);
            hotbar_handle.RegisterMember("is_selected", &HotbarSlotViewModel::is_selected);
        } else {
            spdlog::error("InventoryMenuScene: RegisterStruct<HotbarSlotViewModel> 失败。");
            data_bridge_.destroy();
            return false;
        }

        if (!constructor.RegisterArray<decltype(hotbar_slots_)>()) {
            spdlog::error("InventoryMenuScene: RegisterArray<hotbar> 失败。");
            data_bridge_.destroy();
            return false;
        }

        if (auto action_handle = constructor.RegisterStruct<ActionEntryViewModel>()) {
            action_handle.RegisterMember("action_id", &ActionEntryViewModel::action_id);
            action_handle.RegisterMember("label", &ActionEntryViewModel::label);
            action_handle.RegisterMember("is_destructive", &ActionEntryViewModel::is_destructive);
        } else {
            spdlog::error("InventoryMenuScene: RegisterStruct<ActionEntryViewModel> 失败。");
            data_bridge_.destroy();
            return false;
        }

        if (!constructor.RegisterArray<decltype(action_menu_entries_)>()) {
            spdlog::error("InventoryMenuScene: RegisterArray<actions> 失败。");
            data_bridge_.destroy();
            return false;
        }

        data_types_registered_ = true;
    }

    if (!constructor.Bind("backpack_slots", &backpack_slots_) ||
        !constructor.Bind("hotbar_slots", &hotbar_slots_) ||
        !constructor.Bind("action_menu_entries", &action_menu_entries_)) {
        spdlog::error("InventoryMenuScene: 绑定数组 data model 失败。");
        data_bridge_.destroy();
        return false;
    }

    constructor.Bind("detail_name", &detail_name_);
    constructor.Bind("detail_category", &detail_category_);
    constructor.Bind("detail_description", &detail_description_);
    constructor.Bind("has_detail", &has_detail_);
    constructor.Bind("action_menu_title", &action_menu_title_);
    constructor.Bind("action_menu_left", &action_menu_left_);
    constructor.Bind("action_menu_top", &action_menu_top_);
    constructor.Bind("action_menu_visible", &action_menu_visible_);
    constructor.Bind("char_name", &char_name_);
    constructor.Bind("char_title", &char_title_);
    constructor.Bind("gold_label", &gold_label_);
    constructor.Bind("farm_label", &farm_label_);

    const auto bind_event =
        [this, &constructor](const char* name, void (InventoryMenuScene::*handler)(int, Rml::Event&)) {
            return constructor.BindEventCallback(name,
                [this, handler](Rml::DataModelHandle, Rml::Event& event, const Rml::VariantList& arguments) {
                    (this->*handler)(getSlotArgument(arguments), event);
                });
        };

    if (!bind_event("bp_slot_focus", &InventoryMenuScene::onBpSlotFocus) ||
        !bind_event("bp_slot_mouse_down", &InventoryMenuScene::onBpSlotMouseDown) ||
        !bind_event("bp_slot_mouse_up", &InventoryMenuScene::onBpSlotMouseUp) ||
        !bind_event("bp_slot_hover_enter", &InventoryMenuScene::onBpSlotHoverEnter) ||
        !bind_event("bp_slot_hover_exit", &InventoryMenuScene::onBpSlotHoverExit) ||
        !bind_event("bp_slot_drag_start", &InventoryMenuScene::onBpSlotDragStart) ||
        !bind_event("bp_slot_drag_drop", &InventoryMenuScene::onBpSlotDragDrop) ||
        !bind_event("bp_slot_drag_end", &InventoryMenuScene::onBpSlotDragEnd) ||
        !bind_event("hb_slot_focus", &InventoryMenuScene::onHbSlotFocus) ||
        !bind_event("hb_slot_mouse_down", &InventoryMenuScene::onHbSlotMouseDown) ||
        !bind_event("hb_slot_mouse_up", &InventoryMenuScene::onHbSlotMouseUp) ||
        !bind_event("hb_slot_hover_enter", &InventoryMenuScene::onHbSlotHoverEnter) ||
        !bind_event("hb_slot_hover_exit", &InventoryMenuScene::onHbSlotHoverExit) ||
        !bind_event("hb_slot_drag_start", &InventoryMenuScene::onHbSlotDragStart) ||
        !bind_event("hb_slot_drag_drop", &InventoryMenuScene::onHbSlotDragDrop) ||
        !bind_event("hb_slot_drag_end", &InventoryMenuScene::onHbSlotDragEnd) ||
        !bind_event("action_entry_focus", &InventoryMenuScene::onActionEntryFocus) ||
        !bind_event("action_entry_click", &InventoryMenuScene::onActionEntryClick)) {
        spdlog::error("InventoryMenuScene: 绑定 event 回调失败。");
        data_bridge_.destroy();
        return false;
    }

    document_ = loadRmlDocument(DOCUMENT_PATH);
    if (!document_) {
        data_bridge_.destroy();
        spdlog::error("InventoryMenuScene: 加载 RML 文档失败。");
        return false;
    }

    event_bridge_.on("trash", [this](Rml::Event&) { onTrashClicked(); });
    event_bridge_.on("sort", [this](Rml::Event&) { onSortClicked(); });
    event_bridge_.registerTo(document_, "click");

    tooltip_ui_ = std::make_unique<game::ui::ItemTooltipUI>(context_, instance_id_);

    syncFromInventory();
    syncHotbarFromInventory();
    syncCharacterPanel();
    data_bridge_.markAllDirty();

    runtime->queueFocusFirstEnabledElementByClass(document_, "hb-slot");
    return true;
}

void InventoryMenuScene::beforeUnloadOwnedRmlDocuments() {
    event_bridge_.unregisterAll();
}

void InventoryMenuScene::afterUnloadOwnedRmlDocuments() {
    document_ = nullptr;
    focus_before_action_menu_ = nullptr;
    data_bridge_.destroy();
}

void InventoryMenuScene::disconnectRuntimeListeners() {
    context_.getInputManager().onAction("menu_cancel"_hs)
        .disconnect<&InventoryMenuScene::onMenuCancelPressed>(this);
    context_.getDispatcher().sink<game::defs::InventoryChanged>()
        .disconnect<&InventoryMenuScene::onInventoryChanged>(this);
    context_.getDispatcher().sink<game::defs::HotbarChanged>()
        .disconnect<&InventoryMenuScene::onHotbarChanged>(this);
}

void InventoryMenuScene::syncFromInventory() {
    const auto* inventory = game_registry_.try_get<game::component::InventoryComponent>(player_);
    if (!inventory) {
        return;
    }

    for (int i = 0; i < TOTAL_SLOTS; ++i) {
        const auto& stack = inventory->slot(i);
        auto& vm = backpack_slots_[i];

        if (stack.empty()) {
            vm.icon_decorator = "none";
            vm.count_text.clear();
            vm.has_item = false;
            vm.has_count = false;
            vm.can_drag = false;
        } else {
            vm.icon_decorator = game::ui::buildItemIconDecorator(item_catalog_, stack.item_id_);
            vm.has_item = game::ui::hasDecorator(vm.icon_decorator);
            vm.has_count = stack.count_ > 1;
            vm.count_text = vm.has_count ? std::to_string(stack.count_) : Rml::String{};
            vm.can_drag = true;
        }
    }
}

void InventoryMenuScene::syncHotbarFromInventory() {
    const auto* hotbar = game_registry_.try_get<game::component::HotbarComponent>(player_);
    const auto* inventory = game_registry_.try_get<game::component::InventoryComponent>(player_);

    for (int i = 0; i < HOTBAR_SLOTS; ++i) {
        auto& vm = hotbar_slots_[i];
        vm.is_active = hotbar && hotbar->active_slot_index_ == i;

        if (!hotbar || !inventory || hotbar->slot(i).empty()) {
            vm.icon_decorator = "none";
            vm.count_text.clear();
            vm.has_item = false;
            vm.has_count = false;
            vm.can_drag = false;
            continue;
        }

        const int inventory_slot = hotbar->slot(i).inventory_slot_index_;
        if (inventory_slot < 0 || inventory_slot >= inventory->slotCount()) {
            vm.icon_decorator = "none";
            vm.count_text.clear();
            vm.has_item = false;
            vm.has_count = false;
            vm.can_drag = false;
            continue;
        }

        const auto& stack = inventory->slot(inventory_slot);
        if (stack.empty()) {
            vm.icon_decorator = "none";
            vm.count_text.clear();
            vm.has_item = false;
            vm.has_count = false;
            vm.can_drag = false;
        } else {
            vm.icon_decorator = game::ui::buildItemIconDecorator(item_catalog_, stack.item_id_);
            vm.has_item = game::ui::hasDecorator(vm.icon_decorator);
            vm.has_count = stack.count_ > 1;
            vm.count_text = vm.has_count ? std::to_string(stack.count_) : Rml::String{};
            vm.can_drag = true;
        }
    }
}

void InventoryMenuScene::syncCharacterPanel() {
    char_name_ = "Player";
    if (const auto* name = game_registry_.try_get<engine::component::NameComponent>(player_);
        name && !name->name_.empty()) {
        char_name_ = name->name_;
    }

    char_title_ = "Lv.1 Farmer";
    gold_label_ = "Gold: --";
    farm_label_ = "TinyFarm";

    data_bridge_.markDirty("char_name");
    data_bridge_.markDirty("char_title");
    data_bridge_.markDirty("gold_label");
    data_bridge_.markDirty("farm_label");
}

void InventoryMenuScene::refreshSlot(int slot_index) {
    if (slot_index < 0 || slot_index >= TOTAL_SLOTS) {
        return;
    }

    const auto* inventory = game_registry_.try_get<game::component::InventoryComponent>(player_);
    if (!inventory) {
        return;
    }

    const auto& stack = inventory->slot(slot_index);
    auto& vm = backpack_slots_[slot_index];

    if (stack.empty()) {
        vm.icon_decorator = "none";
        vm.count_text.clear();
        vm.has_item = false;
        vm.has_count = false;
        vm.can_drag = false;
    } else {
        vm.icon_decorator = game::ui::buildItemIconDecorator(item_catalog_, stack.item_id_);
        vm.has_item = game::ui::hasDecorator(vm.icon_decorator);
        vm.has_count = stack.count_ > 1;
        vm.count_text = vm.has_count ? std::to_string(stack.count_) : Rml::String{};
        vm.can_drag = true;
    }
}

void InventoryMenuScene::markSlotsDirty() {
    data_bridge_.markDirty("backpack_slots");
    data_bridge_.markDirty("hotbar_slots");
}

void InventoryMenuScene::markActionMenuDirty() {
    data_bridge_.markDirty("action_menu_entries");
    data_bridge_.markDirty("action_menu_title");
    data_bridge_.markDirty("action_menu_left");
    data_bridge_.markDirty("action_menu_top");
    data_bridge_.markDirty("action_menu_visible");
}

void InventoryMenuScene::showTooltipForInventorySlot(int slot_index) {
    if (!tooltip_ui_ || !item_catalog_ || dragging_ || action_menu_visible_) {
        return;
    }
    if (slot_index < 0 || slot_index >= TOTAL_SLOTS) {
        clearTooltip();
        return;
    }

    const auto* inventory = game_registry_.try_get<game::component::InventoryComponent>(player_);
    if (!inventory) {
        clearTooltip();
        return;
    }

    const auto& stack = inventory->slot(slot_index);
    if (stack.empty()) {
        clearTooltip();
        return;
    }

    const auto* item = item_catalog_->findItem(stack.item_id_);
    if (!item) {
        clearTooltip();
        return;
    }

    tooltip_ui_->showItem(item->display_name_, item->category_str_, item->description_);
}

void InventoryMenuScene::showTooltipForHotbarSlot(int hotbar_index) {
    if (!tooltip_ui_ || !item_catalog_ || dragging_ || action_menu_visible_) {
        return;
    }
    if (hotbar_index < 0 || hotbar_index >= HOTBAR_SLOTS) {
        clearTooltip();
        return;
    }

    const auto* hotbar = game_registry_.try_get<game::component::HotbarComponent>(player_);
    if (!hotbar || hotbar->slot(hotbar_index).empty()) {
        clearTooltip();
        return;
    }

    showTooltipForInventorySlot(hotbar->slot(hotbar_index).inventory_slot_index_);
}

void InventoryMenuScene::clearTooltip() {
    hovered_slot_index_ = -1;
    hovered_hotbar_index_ = -1;
    if (tooltip_ui_) {
        tooltip_ui_->hideTooltip();
    }
}

void InventoryMenuScene::updateDetailForInventorySlot(int slot_index) {
    const auto* item = findItemAtInventorySlot(game_registry_, player_, item_catalog_, slot_index);
    if (!item) {
        clearDetail();
        return;
    }

    detail_name_ = item->display_name_;
    detail_category_ = item->category_str_;
    detail_description_ = item->description_;
    has_detail_ = true;
    data_bridge_.markDirty("detail_name");
    data_bridge_.markDirty("detail_category");
    data_bridge_.markDirty("detail_description");
    data_bridge_.markDirty("has_detail");
}

void InventoryMenuScene::updateDetailForHotbarSlot(int hotbar_index) {
    if (hotbar_index < 0 || hotbar_index >= HOTBAR_SLOTS) {
        clearDetail();
        return;
    }

    const auto* hotbar = game_registry_.try_get<game::component::HotbarComponent>(player_);
    if (!hotbar || hotbar->slot(hotbar_index).empty()) {
        clearDetail();
        return;
    }

    updateDetailForInventorySlot(hotbar->slot(hotbar_index).inventory_slot_index_);
}

void InventoryMenuScene::clearDetail() {
    if (!has_detail_) {
        return;
    }

    has_detail_ = false;
    detail_name_.clear();
    detail_category_.clear();
    detail_description_.clear();
    data_bridge_.markDirty("detail_name");
    data_bridge_.markDirty("detail_category");
    data_bridge_.markDirty("detail_description");
    data_bridge_.markDirty("has_detail");
}

void InventoryMenuScene::selectBpSlot(int slot_index) {
    clearSelection();
    if (slot_index >= 0 && slot_index < TOTAL_SLOTS) {
        backpack_slots_[slot_index].is_selected = true;
    }
    markSlotsDirty();
}

void InventoryMenuScene::selectHbSlot(int slot_index) {
    clearSelection();
    if (slot_index >= 0 && slot_index < HOTBAR_SLOTS) {
        hotbar_slots_[slot_index].is_selected = true;
    }
    markSlotsDirty();
}

void InventoryMenuScene::clearSelection() {
    if (detail_bp_slot_ >= 0 && detail_bp_slot_ < TOTAL_SLOTS) {
        backpack_slots_[detail_bp_slot_].is_selected = false;
    }
    if (detail_hb_slot_ >= 0 && detail_hb_slot_ < HOTBAR_SLOTS) {
        hotbar_slots_[detail_hb_slot_].is_selected = false;
    }
}

void InventoryMenuScene::clearSelectionAndDetail() {
    clearSelection();
    detail_bp_slot_ = -1;
    detail_hb_slot_ = -1;
    markSlotsDirty();
    clearDetail();
}

void InventoryMenuScene::clearDragState() {
    dragging_ = false;
    drop_handled_ = false;
    suppress_next_primary_mouse_up_ = false;
    dragging_from_hotbar_ = false;
    dragging_slot_index_ = -1;
}

void InventoryMenuScene::closeActionMenu(bool restore_focus) {
    if (!action_menu_visible_ && action_menu_entries_.empty()) {
        focus_before_action_menu_ = nullptr;
        highlighted_action_id_ = -1;
        return;
    }

    action_menu_visible_ = false;
    action_menu_entries_.clear();
    action_menu_title_.clear();
    highlighted_action_id_ = -1;
    markActionMenuDirty();

    if (restore_focus) {
        if (auto* runtime = context_.getRmlUi();
            runtime && focus_before_action_menu_ && focus_before_action_menu_->GetOwnerDocument() == document_) {
            runtime->queueFocusElement(focus_before_action_menu_);
        }
    }

    focus_before_action_menu_ = nullptr;
}

void InventoryMenuScene::openBackpackActionMenu(int slot_index) {
    const auto* inventory = game_registry_.try_get<game::component::InventoryComponent>(player_);
    if (!inventory || slot_index < 0 || slot_index >= inventory->slotCount() || inventory->slot(slot_index).empty()) {
        closeActionMenu(false);
        return;
    }

    if (auto* runtime = context_.getRmlUi()) {
        focus_before_action_menu_ = runtime->getFocusedElement();
        if (focus_before_action_menu_ && focus_before_action_menu_->GetOwnerDocument() != document_) {
            focus_before_action_menu_ = nullptr;
        }
    }

    action_menu_entries_.clear();
    if (hasUsableItemAtInventorySlot(game_registry_, player_, item_catalog_, slot_index)) {
        action_menu_entries_.push_back({static_cast<int>(MenuActionId::Use), "Use", false});
    }
    action_menu_entries_.push_back({static_cast<int>(MenuActionId::Discard), "Discard", true});
    action_menu_entries_.push_back({static_cast<int>(MenuActionId::Cancel), "Cancel", false});

    if (const auto* item = findItemAtInventorySlot(game_registry_, player_, item_catalog_, slot_index)) {
        action_menu_title_ = item->display_name_;
    } else {
        action_menu_title_ = "Backpack";
    }

    action_menu_visible_ = true;
    highlighted_action_id_ = action_menu_entries_.empty() ? -1 : action_menu_entries_.front().action_id;
    setActionMenuPositionForBackpackSlot(slot_index);
    markActionMenuDirty();
    clearTooltip();

    if (auto* runtime = context_.getRmlUi()) {
        runtime->queueFocusFirstEnabledElementByClass(document_, "action-menu-entry");
    }
}

void InventoryMenuScene::openHotbarActionMenu(int slot_index) {
    const auto* hotbar = game_registry_.try_get<game::component::HotbarComponent>(player_);
    if (!hotbar || slot_index < 0 || slot_index >= HOTBAR_SLOTS || hotbar->slot(slot_index).empty()) {
        closeActionMenu(false);
        return;
    }

    if (auto* runtime = context_.getRmlUi()) {
        focus_before_action_menu_ = runtime->getFocusedElement();
        if (focus_before_action_menu_ && focus_before_action_menu_->GetOwnerDocument() != document_) {
            focus_before_action_menu_ = nullptr;
        }
    }

    const int inventory_slot = hotbar->slot(slot_index).inventory_slot_index_;

    action_menu_entries_.clear();
    action_menu_entries_.push_back({static_cast<int>(MenuActionId::Activate), "Activate", false});
    if (hasUsableItemAtInventorySlot(game_registry_, player_, item_catalog_, inventory_slot)) {
        action_menu_entries_.push_back({static_cast<int>(MenuActionId::Use), "Use", false});
    }
    action_menu_entries_.push_back({static_cast<int>(MenuActionId::Unbind), "Unbind", false});
    action_menu_entries_.push_back({static_cast<int>(MenuActionId::Cancel), "Cancel", false});

    if (const auto* item = findItemAtInventorySlot(game_registry_, player_, item_catalog_, inventory_slot)) {
        action_menu_title_ = item->display_name_;
    } else {
        action_menu_title_ = "Hotbar";
    }

    action_menu_visible_ = true;
    highlighted_action_id_ = action_menu_entries_.empty() ? -1 : action_menu_entries_.front().action_id;
    setActionMenuPositionForHotbarSlot(slot_index);
    markActionMenuDirty();
    clearTooltip();

    if (auto* runtime = context_.getRmlUi()) {
        runtime->queueFocusFirstEnabledElementByClass(document_, "action-menu-entry");
    }
}

void InventoryMenuScene::openDiscardConfirmForBackpackSlot(int slot_index) {
    const auto* inventory = game_registry_.try_get<game::component::InventoryComponent>(player_);
    if (!inventory || slot_index < 0 || slot_index >= inventory->slotCount()) {
        return;
    }

    const auto& stack = inventory->slot(slot_index);
    if (stack.empty()) {
        return;
    }

    if (!action_menu_visible_) {
        if (auto* runtime = context_.getRmlUi()) {
            focus_before_action_menu_ = runtime->getFocusedElement();
            if (focus_before_action_menu_ && focus_before_action_menu_->GetOwnerDocument() != document_) {
                focus_before_action_menu_ = nullptr;
            }
        }
    }

    action_menu_entries_.clear();
    action_menu_entries_.push_back({static_cast<int>(MenuActionId::ConfirmDiscard), "Discard", true});
    action_menu_entries_.push_back({static_cast<int>(MenuActionId::Cancel), "Cancel", false});

    if (const auto* item = item_catalog_ ? item_catalog_->findItem(stack.item_id_) : nullptr) {
        action_menu_title_ = "Discard " + item->display_name_ + " x" + std::to_string(stack.count_) + "?";
    } else {
        action_menu_title_ = "Discard stack?";
    }

    action_menu_visible_ = true;
    highlighted_action_id_ = action_menu_entries_.front().action_id;
    setActionMenuPositionForBackpackSlot(slot_index);
    markActionMenuDirty();
    clearTooltip();

    if (auto* runtime = context_.getRmlUi()) {
        runtime->queueFocusFirstEnabledElementByClass(document_, "action-menu-entry");
    }
}

void InventoryMenuScene::setActionMenuPositionForBackpackSlot(int slot_index) {
    const int column = slot_index % game::component::InventoryComponent::COLUMNS;
    const int row = slot_index / game::component::InventoryComponent::COLUMNS;
    setActionMenuPosition(
        static_cast<float>(column) * (SLOT_SIZE_DP + SLOT_GAP_DP),
        BACKPACK_TOP_DP + static_cast<float>(row) * (SLOT_SIZE_DP + SLOT_GAP_DP));
}

void InventoryMenuScene::setActionMenuPositionForHotbarSlot(int slot_index) {
    setActionMenuPosition(
        static_cast<float>(slot_index) * (SLOT_SIZE_DP + SLOT_GAP_DP),
        HOTBAR_TOP_DP);
}

void InventoryMenuScene::setActionMenuPosition(float left_dp, float top_dp) {
    const float menu_height =
        ACTION_MENU_HEADER_DP + ACTION_MENU_ENTRY_DP * static_cast<float>(std::max<std::size_t>(1, action_menu_entries_.size()));

    float final_left = left_dp + SLOT_SIZE_DP + ACTION_MENU_GAP_DP;
    if (final_left + ACTION_MENU_WIDTH_DP > INVENTORY_COLUMN_WIDTH_DP) {
        final_left = left_dp - ACTION_MENU_WIDTH_DP - ACTION_MENU_GAP_DP;
    }

    final_left = std::clamp(final_left, 0.0F, std::max(0.0F, INVENTORY_COLUMN_WIDTH_DP - ACTION_MENU_WIDTH_DP));
    const float final_top = std::clamp(top_dp, 0.0F, std::max(0.0F, ACTION_MENU_MAX_TOP_DP - menu_height));

    action_menu_left_ = toDpString(final_left);
    action_menu_top_ = toDpString(final_top);
}

void InventoryMenuScene::executeAction(int action_id) {
    if (player_ == entt::null) {
        closeActionMenu();
        return;
    }

    switch (static_cast<MenuActionId>(action_id)) {
        case MenuActionId::Use: {
            int inventory_slot = detail_bp_slot_;
            if (inventory_slot < 0 && detail_hb_slot_ >= 0) {
                const auto* hotbar = tryGetHotbar(game_registry_, player_);
                if (hotbar && !hotbar->slot(detail_hb_slot_).empty()) {
                    inventory_slot = hotbar->slot(detail_hb_slot_).inventory_slot_index_;
                }
            }

            closeActionMenu();
            if (inventory_slot >= 0) {
                context_.getDispatcher().trigger(game::defs::UseItemCommand{player_, inventory_slot, 1, false});
            }
            break;
        }
        case MenuActionId::Discard:
            if (detail_bp_slot_ >= 0) {
                openDiscardConfirmForBackpackSlot(detail_bp_slot_);
            }
            break;
        case MenuActionId::Activate:
            closeActionMenu();
            if (detail_hb_slot_ >= 0) {
                context_.getDispatcher().trigger(game::defs::HotbarActivateCommand{player_, detail_hb_slot_});
            }
            break;
        case MenuActionId::Unbind:
            closeActionMenu();
            if (detail_hb_slot_ >= 0) {
                context_.getDispatcher().trigger(game::defs::HotbarUnbindCommand{player_, detail_hb_slot_});
            }
            break;
        case MenuActionId::ConfirmDiscard: {
            const auto* inventory = tryGetInventory(game_registry_, player_);
            if (!inventory || detail_bp_slot_ < 0 || detail_bp_slot_ >= inventory->slotCount()) {
                closeActionMenu();
                break;
            }

            const auto& stack = inventory->slot(detail_bp_slot_);
            closeActionMenu();
            if (!stack.empty()) {
                context_.getDispatcher().trigger(game::defs::RemoveItemCommand{
                    player_, stack.item_id_, stack.count_, detail_bp_slot_});
            }
            break;
        }
        case MenuActionId::Cancel:
        default:
            closeActionMenu();
            break;
    }
}

bool InventoryMenuScene::onMenuCancelPressed() {
    if (action_menu_visible_) {
        closeActionMenu();
        return true;
    }

    requestPopScene();
    return true;
}

void InventoryMenuScene::onTrashClicked() {
    if (dragging_ || detail_bp_slot_ < 0) {
        return;
    }

    const auto* inventory = tryGetInventory(game_registry_, player_);
    if (!inventory || detail_bp_slot_ >= inventory->slotCount() || inventory->slot(detail_bp_slot_).empty()) {
        return;
    }

    openDiscardConfirmForBackpackSlot(detail_bp_slot_);
}

void InventoryMenuScene::onSortClicked() {
    if (dragging_ || player_ == entt::null) {
        return;
    }

    closeActionMenu();
    clearTooltip();
    clearSelectionAndDetail();
    context_.getDispatcher().trigger(game::defs::InventorySortCommand{player_});
}

void InventoryMenuScene::onInventoryChanged(const game::defs::InventoryChanged& evt) {
    if (evt.target != player_) {
        return;
    }

    if (evt.full_sync) {
        syncFromInventory();
        closeActionMenu(false);
    } else {
        for (const auto& update : evt.slots) {
            refreshSlot(update.slot_index);
        }
    }
    data_bridge_.markDirty("backpack_slots");

    if (detail_bp_slot_ >= 0) {
        updateDetailForInventorySlot(detail_bp_slot_);
        const auto* inventory = tryGetInventory(game_registry_, player_);
        if (action_menu_visible_ &&
            (!inventory || detail_bp_slot_ >= inventory->slotCount() || inventory->slot(detail_bp_slot_).empty())) {
            closeActionMenu(false);
        }
    } else if (detail_hb_slot_ >= 0) {
        updateDetailForHotbarSlot(detail_hb_slot_);
    }
}

void InventoryMenuScene::onHotbarChanged(const game::defs::HotbarChanged& evt) {
    if (evt.target != player_) {
        return;
    }

    syncHotbarFromInventory();
    data_bridge_.markDirty("hotbar_slots");

    if (detail_hb_slot_ >= 0) {
        updateDetailForHotbarSlot(detail_hb_slot_);

        const auto* hotbar = tryGetHotbar(game_registry_, player_);
        if (action_menu_visible_ &&
            (!hotbar || detail_hb_slot_ >= HOTBAR_SLOTS || hotbar->slot(detail_hb_slot_).empty())) {
            closeActionMenu(false);
        }
    }
}

void InventoryMenuScene::onBpSlotFocus(int slot_index, Rml::Event& /*event*/) {
    if (slot_index < 0 || slot_index >= TOTAL_SLOTS || dragging_ || action_menu_visible_) {
        return;
    }

    selectBpSlot(slot_index);
    detail_bp_slot_ = slot_index;
    detail_hb_slot_ = -1;
    updateDetailForInventorySlot(slot_index);
}

void InventoryMenuScene::onBpSlotMouseDown(int slot_index, Rml::Event& event) {
    if (slot_index < 0 || slot_index >= TOTAL_SLOTS || dragging_) {
        return;
    }

    const int button = event.GetParameter("button", -1);
    if (!engine::ui::rmlui::isSecondaryMouseButton(button)) {
        return;
    }

    event.StopPropagation();
    selectBpSlot(slot_index);
    detail_bp_slot_ = slot_index;
    detail_hb_slot_ = -1;
    updateDetailForInventorySlot(slot_index);

    const auto* inventory = tryGetInventory(game_registry_, player_);
    if (!inventory || slot_index >= inventory->slotCount() || inventory->slot(slot_index).empty()) {
        closeActionMenu(false);
        return;
    }

    openBackpackActionMenu(slot_index);
}

void InventoryMenuScene::onBpSlotMouseUp(int slot_index, Rml::Event& event) {
    if (slot_index < 0 || slot_index >= TOTAL_SLOTS || dragging_) {
        return;
    }

    const int button = event.GetParameter("button", -1);
    if (!engine::ui::rmlui::isPrimaryMouseButton(button)) {
        return;
    }

    event.StopPropagation();
    if (suppress_next_primary_mouse_up_) {
        suppress_next_primary_mouse_up_ = false;
        return;
    }

    selectBpSlot(slot_index);
    detail_bp_slot_ = slot_index;
    detail_hb_slot_ = -1;
    updateDetailForInventorySlot(slot_index);

    const auto* inventory = tryGetInventory(game_registry_, player_);
    if (!inventory || slot_index >= inventory->slotCount() || inventory->slot(slot_index).empty()) {
        closeActionMenu(false);
        return;
    }

    closeActionMenu(false);
}

void InventoryMenuScene::onBpSlotHoverEnter(int slot_index, Rml::Event& event) {
    if (slot_index < 0 || slot_index >= TOTAL_SLOTS || action_menu_visible_) {
        return;
    }

    event.StopPropagation();
    hovered_slot_index_ = slot_index;
    hovered_hotbar_index_ = -1;
    showTooltipForInventorySlot(slot_index);
}

void InventoryMenuScene::onBpSlotHoverExit(int slot_index, Rml::Event& event) {
    if (slot_index < 0 || slot_index >= TOTAL_SLOTS || action_menu_visible_) {
        return;
    }

    event.StopPropagation();
    if (hovered_slot_index_ == slot_index) {
        clearTooltip();
    }
}

void InventoryMenuScene::onBpSlotDragStart(int slot_index, Rml::Event& event) {
    if (slot_index < 0 || slot_index >= TOTAL_SLOTS || player_ == entt::null) {
        return;
    }

    const auto* inventory = game_registry_.try_get<game::component::InventoryComponent>(player_);
    if (!inventory || inventory->slot(slot_index).empty()) {
        return;
    }

    event.StopPropagation();
    closeActionMenu(false);
    clearTooltip();
    dragging_ = true;
    drop_handled_ = false;
    suppress_next_primary_mouse_up_ = true;
    dragging_from_hotbar_ = false;
    dragging_slot_index_ = slot_index;
}

void InventoryMenuScene::onBpSlotDragDrop(int slot_index, Rml::Event& event) {
    if (slot_index < 0 || slot_index >= TOTAL_SLOTS || player_ == entt::null || !dragging_) {
        return;
    }

    event.StopPropagation();
    clearTooltip();
    drop_handled_ = true;

    if (dragging_from_hotbar_) {
        const auto* hotbar = game_registry_.try_get<game::component::HotbarComponent>(player_);
        if (!hotbar || dragging_slot_index_ < 0 || dragging_slot_index_ >= HOTBAR_SLOTS) {
            return;
        }

        const int source_inventory_slot = hotbar->slot(dragging_slot_index_).inventory_slot_index_;
        if (source_inventory_slot >= 0 && source_inventory_slot != slot_index) {
            context_.getDispatcher().trigger(game::defs::InventoryMoveCommand{
                player_, source_inventory_slot, slot_index, true});
        }
        return;
    }

    if (dragging_slot_index_ != slot_index) {
        context_.getDispatcher().trigger(game::defs::InventoryMoveCommand{
            player_, dragging_slot_index_, slot_index, true});
    }
}

void InventoryMenuScene::onBpSlotDragEnd(int slot_index, Rml::Event& event) {
    if (!dragging_ || dragging_from_hotbar_ || slot_index != dragging_slot_index_) {
        return;
    }

    event.StopPropagation();
    clearDragState();
}

void InventoryMenuScene::onHbSlotFocus(int slot_index, Rml::Event& /*event*/) {
    if (slot_index < 0 || slot_index >= HOTBAR_SLOTS || dragging_ || action_menu_visible_) {
        return;
    }

    selectHbSlot(slot_index);
    detail_hb_slot_ = slot_index;
    detail_bp_slot_ = -1;
    updateDetailForHotbarSlot(slot_index);
}

void InventoryMenuScene::onHbSlotMouseDown(int slot_index, Rml::Event& event) {
    if (slot_index < 0 || slot_index >= HOTBAR_SLOTS || player_ == entt::null || dragging_) {
        return;
    }

    const int button = event.GetParameter("button", -1);
    if (!engine::ui::rmlui::isSecondaryMouseButton(button)) {
        return;
    }

    event.StopPropagation();
    selectHbSlot(slot_index);
    detail_hb_slot_ = slot_index;
    detail_bp_slot_ = -1;
    updateDetailForHotbarSlot(slot_index);

    const auto* hotbar = tryGetHotbar(game_registry_, player_);
    if (!hotbar || hotbar->slot(slot_index).empty()) {
        closeActionMenu(false);
        return;
    }

    openHotbarActionMenu(slot_index);
}

void InventoryMenuScene::onHbSlotMouseUp(int slot_index, Rml::Event& event) {
    if (slot_index < 0 || slot_index >= HOTBAR_SLOTS || player_ == entt::null || dragging_) {
        return;
    }

    const int button = event.GetParameter("button", -1);
    if (!engine::ui::rmlui::isPrimaryMouseButton(button)) {
        return;
    }

    event.StopPropagation();
    if (suppress_next_primary_mouse_up_) {
        suppress_next_primary_mouse_up_ = false;
        return;
    }

    selectHbSlot(slot_index);
    detail_hb_slot_ = slot_index;
    detail_bp_slot_ = -1;
    updateDetailForHotbarSlot(slot_index);

    closeActionMenu(false);
    context_.getDispatcher().trigger(game::defs::HotbarActivateCommand{player_, slot_index});
}

void InventoryMenuScene::onHbSlotHoverEnter(int slot_index, Rml::Event& event) {
    if (slot_index < 0 || slot_index >= HOTBAR_SLOTS || action_menu_visible_) {
        return;
    }

    event.StopPropagation();
    hovered_hotbar_index_ = slot_index;
    hovered_slot_index_ = -1;
    showTooltipForHotbarSlot(slot_index);
}

void InventoryMenuScene::onHbSlotHoverExit(int slot_index, Rml::Event& event) {
    if (slot_index < 0 || slot_index >= HOTBAR_SLOTS || action_menu_visible_) {
        return;
    }

    event.StopPropagation();
    if (hovered_hotbar_index_ == slot_index) {
        clearTooltip();
    }
}

void InventoryMenuScene::onHbSlotDragStart(int slot_index, Rml::Event& event) {
    if (slot_index < 0 || slot_index >= HOTBAR_SLOTS || player_ == entt::null) {
        return;
    }

    const auto* hotbar = game_registry_.try_get<game::component::HotbarComponent>(player_);
    if (!hotbar || hotbar->slot(slot_index).empty()) {
        return;
    }

    event.StopPropagation();
    closeActionMenu(false);
    clearTooltip();
    dragging_ = true;
    drop_handled_ = false;
    suppress_next_primary_mouse_up_ = true;
    dragging_from_hotbar_ = true;
    dragging_slot_index_ = slot_index;
}

void InventoryMenuScene::onHbSlotDragDrop(int slot_index, Rml::Event& event) {
    if (slot_index < 0 || slot_index >= HOTBAR_SLOTS || player_ == entt::null || !dragging_) {
        return;
    }

    event.StopPropagation();
    clearTooltip();
    drop_handled_ = true;

    if (dragging_from_hotbar_) {
        if (dragging_slot_index_ == slot_index) {
            return;
        }

        const auto* hotbar = game_registry_.try_get<game::component::HotbarComponent>(player_);
        if (!hotbar) {
            return;
        }

        const int source_inventory_slot = hotbar->slot(dragging_slot_index_).inventory_slot_index_;
        if (hotbar->slot(slot_index).empty()) {
            context_.getDispatcher().trigger(game::defs::HotbarBindCommand{player_, slot_index, source_inventory_slot});
            context_.getDispatcher().trigger(game::defs::HotbarUnbindCommand{player_, dragging_slot_index_});
        } else {
            const int target_inventory_slot = hotbar->slot(slot_index).inventory_slot_index_;
            context_.getDispatcher().trigger(game::defs::HotbarBindCommand{player_, slot_index, source_inventory_slot});
            context_.getDispatcher().trigger(game::defs::HotbarBindCommand{player_, dragging_slot_index_, target_inventory_slot});
        }
        return;
    }

    context_.getDispatcher().trigger(game::defs::HotbarBindCommand{player_, slot_index, dragging_slot_index_});
}

void InventoryMenuScene::onHbSlotDragEnd(int slot_index, Rml::Event& event) {
    if (!dragging_ || !dragging_from_hotbar_ || slot_index != dragging_slot_index_) {
        return;
    }

    event.StopPropagation();
    if (!drop_handled_) {
        context_.getDispatcher().trigger(game::defs::HotbarUnbindCommand{player_, dragging_slot_index_});
    }
    clearDragState();
}

void InventoryMenuScene::onActionEntryFocus(int action_id, Rml::Event& event) {
    event.StopPropagation();
    highlighted_action_id_ = action_id;
}

void InventoryMenuScene::onActionEntryClick(int action_id, Rml::Event& event) {
    event.StopPropagation();
    executeAction(action_id);
}

} // namespace game::scene
