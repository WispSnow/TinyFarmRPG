#include "inventory_menu_scene.h"

#include "game/component/hotbar_component.h"
#include "game/component/inventory_component.h"
#include "game/data/item_catalog.h"
#include "game/defs/commands.h"
#include "game/defs/events.h"
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

    if (!Scene::init()) {
        return false;
    }
    return true;
}

void InventoryMenuScene::update(float delta_time) {
    Scene::update(delta_time);
}

void InventoryMenuScene::clean() {
    disconnectRuntimeListeners();
    removeEventListeners();
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
        } else {
            vm.icon_decorator = game::ui::buildItemIconDecorator(item_catalog_, stack.item_id_);
            vm.has_item = game::ui::hasDecorator(vm.icon_decorator);
            vm.has_count = stack.count_ > 1;
            vm.count_text = vm.has_count ? std::to_string(stack.count_) : Rml::String{};
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
            continue;
        }

        const int inv_idx = hotbar->slot(i).inventory_slot_index_;
        if (inv_idx < 0 || inv_idx >= inventory->slotCount()) {
            vm.icon_decorator = "none";
            vm.count_text.clear();
            vm.has_item = false;
            vm.has_count = false;
            continue;
        }

        const auto& stack = inventory->slot(inv_idx);
        if (stack.empty()) {
            vm.icon_decorator = "none";
            vm.count_text.clear();
            vm.has_item = false;
            vm.has_count = false;
        } else {
            vm.icon_decorator = game::ui::buildItemIconDecorator(item_catalog_, stack.item_id_);
            vm.has_item = game::ui::hasDecorator(vm.icon_decorator);
            vm.has_count = stack.count_ > 1;
            vm.count_text = vm.has_count ? std::to_string(stack.count_) : Rml::String{};
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
    } else {
        vm.icon_decorator = game::ui::buildItemIconDecorator(item_catalog_, stack.item_id_);
        vm.has_item = game::ui::hasDecorator(vm.icon_decorator);
        vm.has_count = stack.count_ > 1;
        vm.count_text = vm.has_count ? std::to_string(stack.count_) : Rml::String{};
    }
}

void InventoryMenuScene::markSlotsDirty() {
    data_bridge_.markDirty("backpack_slots");
    data_bridge_.markDirty("hotbar_slots");
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
        syncHotbarFromInventory();
    } else {
        for (const auto& update : evt.slots) {
            refreshSlot(update.slot_index);
        }
        syncHotbarFromInventory();
    }
    markSlotsDirty();
}

} // namespace game::scene
