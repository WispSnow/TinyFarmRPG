#include "inventory_menu_scene.h"

#include "game/component/hotbar_component.h"
#include "game/component/inventory_component.h"
#include "game/data/item_catalog.h"
#include "game/defs/commands.h"
#include "game/defs/events.h"
#include "game/ui/item_tooltip_ui.h"
#include "game/ui/rml_item_icon_helpers.h"

#include "engine/core/context.h"
#include "engine/core/game_state.h"
#include "engine/input/input_manager.h"
#include "engine/render/opengl/gl_renderer.h"
#include "engine/ui/rmlui/hover_focus_sync_listener.h"
#include "engine/ui/rmlui/rml_ui_layer.h"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/DataTypeRegister.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Event.h>
#include <entt/core/hashed_string.hpp>
#include <entt/signal/dispatcher.hpp>
#include <spdlog/spdlog.h>

#include <string>

using namespace entt::literals;

namespace {

constexpr int TOTAL_SLOTS = game::component::InventoryComponent::TOTAL_SLOTS;
constexpr int HOTBAR_SLOTS = game::component::HotbarComponent::SLOT_COUNT;
constexpr std::string_view DOCUMENT_PATH = "ui/rmlui/scenes/inventory_menu.rml";
constexpr std::string_view MODEL_NAME = "inventory_menu";

[[nodiscard]] int getSlotArgument(const Rml::VariantList& arguments) {
    return (arguments.size() == 1 ? arguments[0].Get<int>(-1) : -1);
}

} // namespace

namespace game::scene {

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

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
    if (document_ || data_bridge_.isValid() || click_listener_registered_ || hover_listener_registered_) {
        removeEventListeners();
        tooltip_ui_.reset();
        if (document_) {
            unloadAllRmlDocuments();
            document_ = nullptr;
        }
        data_bridge_.destroy();
    }
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

    if (!Scene::init()) {
        return false;
    }
    return true;
}

void InventoryMenuScene::update(float delta_time) {
    if (tooltip_ui_) {
        tooltip_ui_->update(delta_time);
    }
    Scene::update(delta_time);
}

void InventoryMenuScene::clean() {
    disconnectRuntimeListeners();
    removeEventListeners();
    clearTooltip();
    tooltip_ui_.reset();
    context_.getGameState().setState(previous_state_);
    if (context_pushed_) {
        context_.getInputManager().popContext();
        context_pushed_ = false;
    }
    Scene::clean();
    document_ = nullptr;
    data_bridge_.destroy();
}

// ---------------------------------------------------------------------------
// UI initialisation
// ---------------------------------------------------------------------------

bool InventoryMenuScene::initUI() {
    auto* layer = context_.getGLRenderer().getRmlUILayer();
    if (!layer) {
        spdlog::error("InventoryMenuScene: RmlUILayer 不可用。");
        return false;
    }

    auto* rml_context = layer->getContext();
    if (!rml_context) {
        spdlog::error("InventoryMenuScene: RmlUi context 不可用。");
        return false;
    }

    // --- data model ---
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
        } else {
            spdlog::error("InventoryMenuScene: RegisterStruct<SlotViewModel> 失败。");
            data_bridge_.destroy();
            return false;
        }
        if (!constructor.RegisterArray<decltype(backpack_slots_)>()) {
            spdlog::error("InventoryMenuScene: RegisterArray 失败。");
            data_bridge_.destroy();
            return false;
        }
        if (auto hslot_handle = constructor.RegisterStruct<HotbarSlotViewModel>()) {
            hslot_handle.RegisterMember("slot_index", &HotbarSlotViewModel::slot_index);
            hslot_handle.RegisterMember("icon_decorator", &HotbarSlotViewModel::icon_decorator);
            hslot_handle.RegisterMember("count_text", &HotbarSlotViewModel::count_text);
            hslot_handle.RegisterMember("label", &HotbarSlotViewModel::label);
            hslot_handle.RegisterMember("has_item", &HotbarSlotViewModel::has_item);
            hslot_handle.RegisterMember("has_count", &HotbarSlotViewModel::has_count);
            hslot_handle.RegisterMember("is_active", &HotbarSlotViewModel::is_active);
            hslot_handle.RegisterMember("can_drag", &HotbarSlotViewModel::can_drag);
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
        data_types_registered_ = true;
    }

    if (!constructor.Bind("backpack_slots", &backpack_slots_)) {
        spdlog::error("InventoryMenuScene: Bind backpack_slots 失败。");
        data_bridge_.destroy();
        return false;
    }
    if (!constructor.Bind("hotbar_slots", &hotbar_slots_)) {
        spdlog::error("InventoryMenuScene: Bind hotbar_slots 失败。");
        data_bridge_.destroy();
        return false;
    }

    // --- slot event callbacks ---
    const auto bind_bp_event =
        [this, &constructor](const char* name, void (InventoryMenuScene::*handler)(int, Rml::Event&)) {
            return constructor.BindEventCallback(name,
                [this, handler](Rml::DataModelHandle, Rml::Event& event, const Rml::VariantList& arguments) {
                    (this->*handler)(getSlotArgument(arguments), event);
                });
        };

    if (!bind_bp_event("bp_slot_hover_enter", &InventoryMenuScene::onBpSlotHoverEnter) ||
        !bind_bp_event("bp_slot_hover_exit",  &InventoryMenuScene::onBpSlotHoverExit) ||
        !bind_bp_event("bp_slot_drag_start",  &InventoryMenuScene::onBpSlotDragStart) ||
        !bind_bp_event("bp_slot_drag_drop",   &InventoryMenuScene::onBpSlotDragDrop) ||
        !bind_bp_event("bp_slot_drag_end",    &InventoryMenuScene::onBpSlotDragEnd) ||
        !bind_bp_event("hb_slot_hover_enter", &InventoryMenuScene::onHbSlotHoverEnter) ||
        !bind_bp_event("hb_slot_hover_exit",  &InventoryMenuScene::onHbSlotHoverExit) ||
        !bind_bp_event("hb_slot_drag_start",  &InventoryMenuScene::onHbSlotDragStart) ||
        !bind_bp_event("hb_slot_drag_drop",   &InventoryMenuScene::onHbSlotDragDrop) ||
        !bind_bp_event("hb_slot_drag_end",    &InventoryMenuScene::onHbSlotDragEnd)) {
        spdlog::error("InventoryMenuScene: 绑定 slot event 回调失败。");
        data_bridge_.destroy();
        return false;
    }

    // --- load document ---
    document_ = loadRmlDocument(DOCUMENT_PATH);
    if (!document_) {
        data_bridge_.destroy();
        spdlog::error("InventoryMenuScene: 加载 RML 文档失败。");
        return false;
    }

    // --- event bridge ---
    event_bridge_.on("close", [this](Rml::Event&) { onCloseClicked(); });
    event_bridge_.registerTo(document_, "click");
    click_listener_registered_ = true;

    hover_focus_listener_ = std::make_unique<engine::ui::rmlui::HoverFocusSyncListener>(*layer);
    document_->AddEventListener("mouseover", hover_focus_listener_.get());
    hover_listener_registered_ = true;

    // --- tooltip ---
    tooltip_ui_ = std::make_unique<game::ui::ItemTooltipUI>(context_, instance_id_);

    // --- populate data from player inventory ---
    syncFromInventory();
    syncHotbarFromInventory();
    data_bridge_.markAllDirty();

    layer->queueFocusFirstEnabledElementByClass(document_, "hb-slot");
    return true;
}

void InventoryMenuScene::removeEventListeners() {
    if (document_ && click_listener_registered_) {
        document_->RemoveEventListener("click", &event_bridge_);
        click_listener_registered_ = false;
    }
    if (document_ && hover_listener_registered_ && hover_focus_listener_) {
        document_->RemoveEventListener("mouseover", hover_focus_listener_.get());
        hover_listener_registered_ = false;
    }
}

void InventoryMenuScene::disconnectRuntimeListeners() {
    context_.getInputManager().onAction("menu_cancel"_hs)
        .disconnect<&InventoryMenuScene::onMenuCancelPressed>(this);
    context_.getDispatcher().sink<game::defs::InventoryChanged>()
        .disconnect<&InventoryMenuScene::onInventoryChanged>(this);
    context_.getDispatcher().sink<game::defs::HotbarChanged>()
        .disconnect<&InventoryMenuScene::onHotbarChanged>(this);
}

// ---------------------------------------------------------------------------
// Inventory sync
// ---------------------------------------------------------------------------

void InventoryMenuScene::syncFromInventory() {
    auto* inventory = game_registry_.try_get<game::component::InventoryComponent>(player_);
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
    auto* hotbar = game_registry_.try_get<game::component::HotbarComponent>(player_);
    auto* inventory = game_registry_.try_get<game::component::InventoryComponent>(player_);

    for (int i = 0; i < HOTBAR_SLOTS; ++i) {
        auto& vm = hotbar_slots_[i];
        vm.is_active = hotbar && (hotbar->active_slot_index_ == i);

        if (!hotbar || hotbar->slot(i).empty() || !inventory) {
            vm.icon_decorator = "none";
            vm.count_text.clear();
            vm.has_item = false;
            vm.has_count = false;
            vm.can_drag = false;
            continue;
        }

        const int inv_idx = hotbar->slot(i).inventory_slot_index_;
        if (inv_idx < 0 || inv_idx >= inventory->slotCount()) {
            vm.icon_decorator = "none";
            vm.count_text.clear();
            vm.has_item = false;
            vm.has_count = false;
            vm.can_drag = false;
            continue;
        }

        const auto& stack = inventory->slot(inv_idx);
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

void InventoryMenuScene::refreshSlot(int slot_index) {
    if (slot_index < 0 || slot_index >= TOTAL_SLOTS) {
        return;
    }

    auto* inventory = game_registry_.try_get<game::component::InventoryComponent>(player_);
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

// ---------------------------------------------------------------------------
// Tooltip
// ---------------------------------------------------------------------------

void InventoryMenuScene::showTooltipForInventorySlot(int slot_index) {
    if (!tooltip_ui_ || !item_catalog_ || dragging_) {
        return;
    }
    if (slot_index < 0 || slot_index >= TOTAL_SLOTS) {
        clearTooltip();
        return;
    }

    auto* inventory = game_registry_.try_get<game::component::InventoryComponent>(player_);
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
    if (!tooltip_ui_ || !item_catalog_ || dragging_) {
        return;
    }
    if (hotbar_index < 0 || hotbar_index >= HOTBAR_SLOTS) {
        clearTooltip();
        return;
    }

    auto* hotbar = game_registry_.try_get<game::component::HotbarComponent>(player_);
    if (!hotbar || hotbar->slot(hotbar_index).empty()) {
        clearTooltip();
        return;
    }

    const int inv_idx = hotbar->slot(hotbar_index).inventory_slot_index_;
    showTooltipForInventorySlot(inv_idx);
}

void InventoryMenuScene::clearTooltip() {
    hovered_slot_index_ = -1;
    hovered_hotbar_index_ = -1;
    if (tooltip_ui_) {
        tooltip_ui_->hideTooltip();
    }
}

// ---------------------------------------------------------------------------
// Drag
// ---------------------------------------------------------------------------

void InventoryMenuScene::clearDragState() {
    dragging_ = false;
    drop_handled_ = false;
    dragging_from_hotbar_ = false;
    dragging_slot_index_ = -1;
}

// ---------------------------------------------------------------------------
// Actions
// ---------------------------------------------------------------------------

bool InventoryMenuScene::onMenuCancelPressed() {
    requestPopScene();
    return true;
}

void InventoryMenuScene::onCloseClicked() {
    requestPopScene();
}

void InventoryMenuScene::onInventoryChanged(const game::defs::InventoryChanged& evt) {
    if (evt.target != player_) {
        return;
    }

    if (evt.full_sync) {
        syncFromInventory();
    } else {
        for (const auto& update : evt.slots) {
            refreshSlot(update.slot_index);
        }
    }
    data_bridge_.markDirty("backpack_slots");
}

void InventoryMenuScene::onHotbarChanged(const game::defs::HotbarChanged& evt) {
    if (evt.target != player_) {
        return;
    }
    syncHotbarFromInventory();
    data_bridge_.markDirty("hotbar_slots");
}

// ---------------------------------------------------------------------------
// Backpack slot events
// ---------------------------------------------------------------------------

void InventoryMenuScene::onBpSlotHoverEnter(int slot_index, Rml::Event& event) {
    if (slot_index < 0 || slot_index >= TOTAL_SLOTS) {
        return;
    }
    event.StopPropagation();
    hovered_slot_index_ = slot_index;
    hovered_hotbar_index_ = -1;
    showTooltipForInventorySlot(slot_index);
}

void InventoryMenuScene::onBpSlotHoverExit(int slot_index, Rml::Event& event) {
    if (slot_index < 0 || slot_index >= TOTAL_SLOTS) {
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
    auto* inventory = game_registry_.try_get<game::component::InventoryComponent>(player_);
    if (!inventory || inventory->slot(slot_index).empty()) {
        return;
    }
    event.StopPropagation();
    clearTooltip();
    dragging_ = true;
    drop_handled_ = false;
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
        // Hotbar → Backpack: move the bound inventory item to target backpack slot
        auto* hotbar = game_registry_.try_get<game::component::HotbarComponent>(player_);
        if (!hotbar || dragging_slot_index_ < 0 || dragging_slot_index_ >= HOTBAR_SLOTS) {
            return;
        }
        const int src_inv = hotbar->slot(dragging_slot_index_).inventory_slot_index_;
        if (src_inv >= 0 && src_inv != slot_index) {
            context_.getDispatcher().trigger(
                game::defs::InventoryMoveCommand{player_, src_inv, slot_index, true});
        }
    } else {
        // Backpack → Backpack: move/swap/merge
        if (dragging_slot_index_ != slot_index) {
            context_.getDispatcher().trigger(
                game::defs::InventoryMoveCommand{player_, dragging_slot_index_, slot_index, true});
        }
    }
}

void InventoryMenuScene::onBpSlotDragEnd(int slot_index, Rml::Event& event) {
    if (!dragging_ || dragging_from_hotbar_ || slot_index != dragging_slot_index_) {
        return;
    }
    event.StopPropagation();
    // Backpack drag-to-nowhere: no-op (items don't get destroyed)
    clearDragState();
}

// ---------------------------------------------------------------------------
// Hotbar slot events
// ---------------------------------------------------------------------------

void InventoryMenuScene::onHbSlotHoverEnter(int slot_index, Rml::Event& event) {
    if (slot_index < 0 || slot_index >= HOTBAR_SLOTS) {
        return;
    }
    event.StopPropagation();
    hovered_hotbar_index_ = slot_index;
    hovered_slot_index_ = -1;
    showTooltipForHotbarSlot(slot_index);
}

void InventoryMenuScene::onHbSlotHoverExit(int slot_index, Rml::Event& event) {
    if (slot_index < 0 || slot_index >= HOTBAR_SLOTS) {
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
    auto* hotbar = game_registry_.try_get<game::component::HotbarComponent>(player_);
    if (!hotbar || hotbar->slot(slot_index).empty()) {
        return;
    }
    event.StopPropagation();
    clearTooltip();
    dragging_ = true;
    drop_handled_ = false;
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
        // Hotbar → Hotbar: swap or move bindings
        if (dragging_slot_index_ == slot_index) {
            return;
        }
        auto* hotbar = game_registry_.try_get<game::component::HotbarComponent>(player_);
        if (!hotbar) {
            return;
        }
        const int src_inv = hotbar->slot(dragging_slot_index_).inventory_slot_index_;
        if (hotbar->slot(slot_index).empty()) {
            // Target empty: bind source's item to target, unbind source
            context_.getDispatcher().trigger(
                game::defs::HotbarBindCommand{player_, slot_index, src_inv});
            context_.getDispatcher().trigger(
                game::defs::HotbarUnbindCommand{player_, dragging_slot_index_});
        } else {
            // Both occupied: cross-bind to swap
            const int tgt_inv = hotbar->slot(slot_index).inventory_slot_index_;
            context_.getDispatcher().trigger(
                game::defs::HotbarBindCommand{player_, slot_index, src_inv});
            context_.getDispatcher().trigger(
                game::defs::HotbarBindCommand{player_, dragging_slot_index_, tgt_inv});
        }
    } else {
        // Backpack → Hotbar: bind backpack item to this hotbar slot
        context_.getDispatcher().trigger(
            game::defs::HotbarBindCommand{player_, slot_index, dragging_slot_index_});
    }
}

void InventoryMenuScene::onHbSlotDragEnd(int slot_index, Rml::Event& event) {
    if (!dragging_ || !dragging_from_hotbar_ || slot_index != dragging_slot_index_) {
        return;
    }
    event.StopPropagation();
    if (!drop_handled_) {
        // Hotbar drag-to-nowhere: unbind
        context_.getDispatcher().trigger(
            game::defs::HotbarUnbindCommand{player_, dragging_slot_index_});
    }
    clearDragState();
}

} // namespace game::scene
