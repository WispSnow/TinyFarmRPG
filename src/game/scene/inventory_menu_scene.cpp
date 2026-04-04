#include "inventory_menu_scene.h"

#include "engine/component/name_component.h"
#include "engine/core/context.h"
#include "engine/core/game_state.h"
#include "engine/input/input_manager.h"
#include "engine/ui/rmlui/rml_element_helpers.h"
#include "engine/ui/rmlui/rml_mouse_buttons.h"
#include "engine/ui/rmlui/rml_ui_runtime.h"
#include "game/component/hotbar_component.h"
#include "game/component/inventory_component.h"
#include "game/data/item_catalog.h"
#include "game/defs/commands.h"
#include "game/defs/events.h"
#include "game/ui/item_tooltip_ui.h"

#include <RmlUi/Core/DataTypeRegister.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Event.h>
#include <entt/core/hashed_string.hpp>
#include <entt/signal/dispatcher.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>

using namespace entt::literals;

namespace {

using engine::ui::rmlui::setPixelProperty;

constexpr int TOTAL_SLOTS = game::component::InventoryComponent::TOTAL_SLOTS;
constexpr int HOTBAR_SLOTS = game::component::HotbarComponent::SLOT_COUNT;
constexpr std::string_view DOCUMENT_PATH = "ui/rmlui/scenes/inventory_menu.rml";
constexpr std::string_view MODEL_NAME = "inventory_menu";

enum class MenuActionId : int {
    Use = 1,
    Discard,
    Activate,
    Unbind,
    ConfirmDiscard,
    Cancel
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
    action_menu_entries_.clear();
    action_menu_visible_ = false;
    tooltip_ui_.reset();
    shutdownUI();
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
    shutdownUI();
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

    document_controller_.attach(runtime, instanceId());
    auto constructor = document_controller_.createModel(MODEL_NAME, &type_register_);
    if (!constructor) {
        spdlog::error("InventoryMenuScene: 创建 data model 失败。");
        return false;
    }

    if (!data_types_registered_) {
        if (!game::ui::registerSlotGridViewModelType(constructor)) {
            spdlog::error("InventoryMenuScene: RegisterStruct<SlotGridViewModel> 失败。");
            document_controller_.unload();
            return false;
        }

        // `backpack_slots_` 与 `hotbar_slots_` 现在共享同一个 vector<SlotGridViewModel> 类型，
        // 这个数组类型只需要注册一次；重复注册会被 RmlUi 视为失败。
        if (!constructor.RegisterArray<decltype(backpack_slots_)>()) {
            spdlog::error("InventoryMenuScene: RegisterArray<backpack> 失败。");
            document_controller_.unload();
            return false;
        }

        if (auto action_handle = constructor.RegisterStruct<ActionEntryViewModel>()) {
            action_handle.RegisterMember("action_id", &ActionEntryViewModel::action_id);
            action_handle.RegisterMember("label", &ActionEntryViewModel::label);
            action_handle.RegisterMember("is_destructive", &ActionEntryViewModel::is_destructive);
        } else {
            spdlog::error("InventoryMenuScene: RegisterStruct<ActionEntryViewModel> 失败。");
            document_controller_.unload();
            return false;
        }

        if (!constructor.RegisterArray<decltype(action_menu_entries_)>()) {
            spdlog::error("InventoryMenuScene: RegisterArray<actions> 失败。");
            document_controller_.unload();
            return false;
        }

        data_types_registered_ = true;
    }

    if (!constructor.Bind("backpack_slots", &backpack_slots_) ||
        !constructor.Bind("hotbar_slots", &hotbar_slots_) ||
        !constructor.Bind("action_menu_entries", &action_menu_entries_)) {
        spdlog::error("InventoryMenuScene: 绑定数组 data model 失败。");
        document_controller_.unload();
        return false;
    }

    constructor.Bind("detail_name", &detail_name_);
    constructor.Bind("detail_category", &detail_category_);
    constructor.Bind("detail_description", &detail_description_);
    constructor.Bind("has_detail", &has_detail_);
    constructor.Bind("action_menu_title", &action_menu_title_);
    constructor.Bind("action_menu_visible", &action_menu_visible_);
    constructor.Bind("char_name", &char_name_);
    constructor.Bind("char_title", &char_title_);
    constructor.Bind("gold_label", &gold_label_);
    constructor.Bind("farm_label", &farm_label_);

    if (!game::ui::bindSlotGridEvents(
            constructor,
            "bp_slot",
            this,
            game::ui::SlotGridEventHandlers<InventoryMenuScene>{
                .on_focus = &InventoryMenuScene::onBpSlotFocus,
                .on_mouse_down = &InventoryMenuScene::onBpSlotMouseDown,
                .on_mouse_up = &InventoryMenuScene::onBpSlotMouseUp,
                .on_hover_enter = &InventoryMenuScene::onBpSlotHoverEnter,
                .on_hover_exit = &InventoryMenuScene::onBpSlotHoverExit,
                .on_drag_start = &InventoryMenuScene::onBpSlotDragStart,
                .on_drag_drop = &InventoryMenuScene::onBpSlotDragDrop,
                .on_drag_end = &InventoryMenuScene::onBpSlotDragEnd,
            }) ||
        !game::ui::bindSlotGridEvents(
            constructor,
            "hb_slot",
            this,
            game::ui::SlotGridEventHandlers<InventoryMenuScene>{
                .on_focus = &InventoryMenuScene::onHbSlotFocus,
                .on_mouse_down = &InventoryMenuScene::onHbSlotMouseDown,
                .on_mouse_up = &InventoryMenuScene::onHbSlotMouseUp,
                .on_hover_enter = &InventoryMenuScene::onHbSlotHoverEnter,
                .on_hover_exit = &InventoryMenuScene::onHbSlotHoverExit,
                .on_drag_start = &InventoryMenuScene::onHbSlotDragStart,
                .on_drag_drop = &InventoryMenuScene::onHbSlotDragDrop,
                .on_drag_end = &InventoryMenuScene::onHbSlotDragEnd,
            }) ||
        !game::ui::bindIndexedEventCallbacks(
            constructor,
            this,
            {
                {"action_entry_click", &InventoryMenuScene::onActionEntryClick},
            }) ||
        !document_controller_.bindSimpleEvent(constructor, "trash", [this] { onTrashClicked(); }) ||
        !document_controller_.bindSimpleEvent(constructor, "sort", [this] { onSortClicked(); })) {
        spdlog::error("InventoryMenuScene: 绑定 event 回调失败。");
        document_controller_.unload();
        return false;
    }

    if (!document_controller_.load(DOCUMENT_PATH)) {
        document_controller_.unload();
        spdlog::error("InventoryMenuScene: 加载 RML 文档失败。");
        return false;
    }

    tooltip_ui_ = std::make_unique<game::ui::ItemTooltipUI>(context_, instanceId());

    syncFromInventory();
    syncHotbarFromInventory();
    syncCharacterPanel();
    document_controller_.markAllDirty();
    return true;
}

void InventoryMenuScene::shutdownUI() {
    document_controller_.unload();
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
        auto& vm = backpack_slots_[i];
        vm.slot_index = i;
        game::ui::populateSlotGridViewModel(
            vm,
            toSlotItem(inventory->slot(i)),
            item_catalog_,
            game::ui::SlotGridViewModelOptions{
                .can_drag = true,
                .is_selected = vm.is_selected,
            });
    }
}

void InventoryMenuScene::syncHotbarFromInventory() {
    const auto* hotbar = game_registry_.try_get<game::component::HotbarComponent>(player_);
    const auto* inventory = game_registry_.try_get<game::component::InventoryComponent>(player_);

    for (int i = 0; i < HOTBAR_SLOTS; ++i) {
        auto& vm = hotbar_slots_[i];
        vm.slot_index = i;

        if (!hotbar || !inventory || hotbar->slot(i).empty()) {
            game::ui::populateSlotGridViewModel(
                vm,
                std::nullopt,
                item_catalog_,
                game::ui::SlotGridViewModelOptions{
                    .is_selected = vm.is_selected,
                    .is_active = false,
                    .label = vm.label,
                });
            continue;
        }

        const int inventory_slot = hotbar->slot(i).inventory_slot_index_;
        if (inventory_slot < 0 || inventory_slot >= inventory->slotCount()) {
            game::ui::populateSlotGridViewModel(
                vm,
                std::nullopt,
                item_catalog_,
                game::ui::SlotGridViewModelOptions{
                    .is_selected = vm.is_selected,
                    .is_active = false,
                    .label = vm.label,
                });
            continue;
        }

        game::ui::populateSlotGridViewModel(
            vm,
            toSlotItem(inventory->slot(inventory_slot)),
            item_catalog_,
            game::ui::SlotGridViewModelOptions{
                .can_drag = true,
                .is_selected = vm.is_selected,
                .is_active = (hotbar->active_slot_index_ == i),
                .label = vm.label,
            });
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

    document_controller_.markDirty("char_name");
    document_controller_.markDirty("char_title");
    document_controller_.markDirty("gold_label");
    document_controller_.markDirty("farm_label");
}

void InventoryMenuScene::refreshSlot(int slot_index) {
    if (slot_index < 0 || slot_index >= TOTAL_SLOTS) {
        return;
    }

    const auto* inventory = game_registry_.try_get<game::component::InventoryComponent>(player_);
    if (!inventory) {
        return;
    }

    auto& vm = backpack_slots_[slot_index];
    vm.slot_index = slot_index;
    game::ui::populateSlotGridViewModel(
        vm,
        toSlotItem(inventory->slot(slot_index)),
        item_catalog_,
        game::ui::SlotGridViewModelOptions{
            .can_drag = true,
            .is_selected = vm.is_selected,
        });
}

void InventoryMenuScene::markSlotsDirty() {
    document_controller_.markDirty("backpack_slots");
    document_controller_.markDirty("hotbar_slots");
}

void InventoryMenuScene::markActionMenuDirty() {
    document_controller_.markDirty("action_menu_entries");
    document_controller_.markDirty("action_menu_title");
    document_controller_.markDirty("action_menu_visible");
}

void InventoryMenuScene::showTooltipForInventorySlot(int slot_index) {
    if (!tooltip_ui_ || !item_catalog_ || drag_state_.active || action_menu_visible_) {
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
    if (!tooltip_ui_ || !item_catalog_ || drag_state_.active || action_menu_visible_) {
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
    document_controller_.markDirty("detail_name");
    document_controller_.markDirty("detail_category");
    document_controller_.markDirty("detail_description");
    document_controller_.markDirty("has_detail");
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
    document_controller_.markDirty("detail_name");
    document_controller_.markDirty("detail_category");
    document_controller_.markDirty("detail_description");
    document_controller_.markDirty("has_detail");
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
    drag_state_.clear();
}

Rml::Element* InventoryMenuScene::findIndexedChildElement(std::string_view parent_id, int child_index) const {
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

float InventoryMenuScene::measureGridHorizontalGap(std::string_view grid_id) const {
    auto* first_slot = findIndexedChildElement(grid_id, 0);
    auto* second_slot = findIndexedChildElement(grid_id, 1);
    if (!first_slot || !second_slot) {
        return 0.0F;
    }

    return std::max(0.0F, second_slot->GetAbsoluteLeft() - first_slot->GetAbsoluteLeft() - first_slot->GetOffsetWidth());
}

void InventoryMenuScene::closeActionMenu(bool /*restore_focus*/) {
    if (!action_menu_visible_ && action_menu_entries_.empty()) {
        return;
    }

    action_menu_visible_ = false;
    action_menu_entries_.clear();
    action_menu_title_.clear();
    markActionMenuDirty();
}

void InventoryMenuScene::positionActionMenuForGridSlot(std::string_view grid_id, int slot_index) {
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

void InventoryMenuScene::showActionMenu(Rml::String title,
                                        std::vector<ActionEntryViewModel> entries,
                                        std::string_view anchor_grid_id,
                                        int anchor_slot_index) {
    if (entries.empty()) {
        closeActionMenu(false);
        return;
    }

    action_menu_entries_ = std::move(entries);
    action_menu_title_ = std::move(title);
    action_menu_visible_ = true;
    markActionMenuDirty();
    clearTooltip();

    if (auto* runtime = context_.getRmlUi()) {
        // `data-if` / `data-for` materialize on context update; update once before measuring the menu box.
        runtime->update();
        positionActionMenuForGridSlot(anchor_grid_id, anchor_slot_index);
    }
}

void InventoryMenuScene::openBackpackActionMenu(int slot_index) {
    const auto* inventory = game_registry_.try_get<game::component::InventoryComponent>(player_);
    if (!inventory || slot_index < 0 || slot_index >= inventory->slotCount() || inventory->slot(slot_index).empty()) {
        closeActionMenu(false);
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

void InventoryMenuScene::openHotbarActionMenu(int slot_index) {
    const auto* hotbar = game_registry_.try_get<game::component::HotbarComponent>(player_);
    if (!hotbar || slot_index < 0 || slot_index >= HOTBAR_SLOTS || hotbar->slot(slot_index).empty()) {
        closeActionMenu(false);
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

void InventoryMenuScene::openDiscardConfirmForBackpackSlot(int slot_index) {
    const auto* inventory = game_registry_.try_get<game::component::InventoryComponent>(player_);
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
    if (drag_state_.active || detail_bp_slot_ < 0) {
        return;
    }

    const auto* inventory = tryGetInventory(game_registry_, player_);
    if (!inventory || detail_bp_slot_ >= inventory->slotCount() || inventory->slot(detail_bp_slot_).empty()) {
        return;
    }

    openDiscardConfirmForBackpackSlot(detail_bp_slot_);
}

void InventoryMenuScene::onSortClicked() {
    if (drag_state_.active || player_ == entt::null) {
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
    document_controller_.markDirty("backpack_slots");

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
    document_controller_.markDirty("hotbar_slots");

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
    if (slot_index < 0 || slot_index >= TOTAL_SLOTS || drag_state_.active || action_menu_visible_) {
        return;
    }

    selectBpSlot(slot_index);
    detail_bp_slot_ = slot_index;
    detail_hb_slot_ = -1;
    updateDetailForInventorySlot(slot_index);
}

void InventoryMenuScene::onBpSlotMouseDown(int slot_index, Rml::Event& event) {
    if (slot_index < 0 || slot_index >= TOTAL_SLOTS || drag_state_.active) {
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
    if (slot_index < 0 || slot_index >= TOTAL_SLOTS || drag_state_.active) {
        return;
    }

    const int button = event.GetParameter("button", -1);
    if (!engine::ui::rmlui::isPrimaryMouseButton(button)) {
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
    drag_state_.start();
}

void InventoryMenuScene::onBpSlotDragDrop(int slot_index, Rml::Event& event) {
    if (slot_index < 0 || slot_index >= TOTAL_SLOTS || player_ == entt::null || !drag_state_.active) {
        return;
    }

    const auto drag_info = game::ui::getSlotGridDragInfo(event);
    if (!drag_info) {
        return;
    }

    event.StopPropagation();
    clearTooltip();
    drag_state_.drop_handled = true;

    if (drag_info->fromHotbar()) {
        const auto* hotbar = game_registry_.try_get<game::component::HotbarComponent>(player_);
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
        context_.getDispatcher().trigger(game::defs::InventoryMoveCommand{player_, drag_info->slot_index, slot_index, true});
    }
}

void InventoryMenuScene::onBpSlotDragEnd(int slot_index, Rml::Event& event) {
    const auto drag_info = game::ui::getSlotGridDragInfo(event);
    if (!drag_state_.active || !drag_info || !drag_info->fromInventory() || slot_index != drag_info->slot_index) {
        return;
    }

    event.StopPropagation();
    clearDragState();
}

void InventoryMenuScene::onHbSlotFocus(int slot_index, Rml::Event& /*event*/) {
    if (slot_index < 0 || slot_index >= HOTBAR_SLOTS || drag_state_.active || action_menu_visible_) {
        return;
    }

    selectHbSlot(slot_index);
    detail_hb_slot_ = slot_index;
    detail_bp_slot_ = -1;
    updateDetailForHotbarSlot(slot_index);
}

void InventoryMenuScene::onHbSlotMouseDown(int slot_index, Rml::Event& event) {
    if (slot_index < 0 || slot_index >= HOTBAR_SLOTS || player_ == entt::null || drag_state_.active) {
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
    if (slot_index < 0 || slot_index >= HOTBAR_SLOTS || player_ == entt::null || drag_state_.active) {
        return;
    }

    const int button = event.GetParameter("button", -1);
    if (!engine::ui::rmlui::isPrimaryMouseButton(button)) {
        return;
    }

    event.StopPropagation();
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
    drag_state_.start();
}

void InventoryMenuScene::onHbSlotDragDrop(int slot_index, Rml::Event& event) {
    if (slot_index < 0 || slot_index >= HOTBAR_SLOTS || player_ == entt::null || !drag_state_.active) {
        return;
    }

    const auto drag_info = game::ui::getSlotGridDragInfo(event);
    if (!drag_info) {
        return;
    }

    event.StopPropagation();
    clearTooltip();
    drag_state_.drop_handled = true;

    if (drag_info->fromHotbar()) {
        if (drag_info->slot_index == slot_index) {
            return;
        }

        const auto* hotbar = game_registry_.try_get<game::component::HotbarComponent>(player_);
        if (!hotbar) {
            return;
        }

        const int source_inventory_slot = hotbar->slot(drag_info->slot_index).inventory_slot_index_;
        if (hotbar->slot(slot_index).empty()) {
            // onBind 会自动清理旧 hotbar 槽位上的同一 inventory 引用，这里不必再显式 unbind。
            context_.getDispatcher().trigger(game::defs::HotbarBindCommand{player_, slot_index, source_inventory_slot});
        } else {
            const int target_inventory_slot = hotbar->slot(slot_index).inventory_slot_index_;
            context_.getDispatcher().trigger(game::defs::HotbarBindCommand{player_, slot_index, source_inventory_slot});
            context_.getDispatcher().trigger(game::defs::HotbarBindCommand{player_, drag_info->slot_index, target_inventory_slot});
        }
        return;
    }

    context_.getDispatcher().trigger(game::defs::HotbarBindCommand{player_, slot_index, drag_info->slot_index});
}

void InventoryMenuScene::onHbSlotDragEnd(int slot_index, Rml::Event& event) {
    const auto drag_info = game::ui::getSlotGridDragInfo(event);
    if (!drag_state_.active || !drag_info || !drag_info->fromHotbar() || slot_index != drag_info->slot_index) {
        return;
    }

    event.StopPropagation();
    if (!drag_state_.drop_handled) {
        context_.getDispatcher().trigger(game::defs::HotbarUnbindCommand{player_, drag_info->slot_index});
    }
    clearDragState();
}

void InventoryMenuScene::onActionEntryClick(int action_id, Rml::Event& event) {
    event.StopPropagation();
    executeAction(action_id);
}

} // namespace game::scene
