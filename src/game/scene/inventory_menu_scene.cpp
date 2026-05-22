#include "inventory_menu_scene.h"

#include "engine/core/context.h"
#include "engine/core/game_state.h"
#include "engine/input/input_manager.h"
#include "engine/ui/rmlui/rml_element_helpers.h"
#include "engine/ui/rmlui/rml_ui_runtime.h"
#include "game/data/item_catalog.h"
#include "game/data/quest_catalog.h"
#include "game/data/rpg_catalog.h"
#include "game/data/shop_catalog.h"
#include "game/defs/commands.h"
#include "game/runtime/user_settings_service.h"
#include "game/scene/inventory_menu_character_panel.h"
#include "game/ui/equipment_tab_content.h"
#include "game/ui/inventory_menu_tab_shortcuts.h"
#include "game/ui/inventory_tab_content.h"
#include "game/ui/map_tab_content.h"
#include "game/ui/options_tab_content.h"
#include "game/ui/quest_tab_content.h"
#include "game/ui/slot_grid_support.h"

#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/DataTypeRegister.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Elements/ElementTabSet.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/Traits.h>
#include <entt/core/hashed_string.hpp>
#include <entt/signal/dispatcher.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

using namespace entt::literals;

namespace {

constexpr std::string_view DOCUMENT_PATH = "ui/rmlui/scenes/inventory_menu.rml";
constexpr std::string_view MODEL_NAME = "inventory_menu";
using SlotGridViewModels = std::vector<game::ui::SlotGridViewModel>;
using PartyMemberPanelViewModels = std::vector<game::scene::PartyMemberPanelViewModel>;
using engine::ui::rmlui::setPixelProperty;
using engine::ui::rmlui::snapToPixel;
using engine::ui::rmlui::textToInnerRml;

[[nodiscard]] std::optional<game::ui::MenuTabId> toMenuTabId(int tab_index) {
    switch (tab_index) {
        case 0:
            return game::ui::MenuTabId::Inventory;
        case 1:
            return game::ui::MenuTabId::Equipment;
        case 2:
            return game::ui::MenuTabId::Quests;
        case 3:
            return game::ui::MenuTabId::Map;
        case 4:
            return game::ui::MenuTabId::Options;
        default:
            return std::nullopt;
    }
}

[[nodiscard]] bool registerPartyMemberPanelViewModelType(Rml::DataModelConstructor& constructor) {
    if (auto handle = constructor.RegisterStruct<game::scene::PartyMemberPanelViewModel>()) {
        handle.RegisterMember("slot_index", &game::scene::PartyMemberPanelViewModel::slot_index);
        handle.RegisterMember("actor_id", &game::scene::PartyMemberPanelViewModel::actor_id);
        handle.RegisterMember("display_name", &game::scene::PartyMemberPanelViewModel::display_name);
        handle.RegisterMember("class_label", &game::scene::PartyMemberPanelViewModel::class_label);
        handle.RegisterMember("level_label", &game::scene::PartyMemberPanelViewModel::level_label);
        handle.RegisterMember("exp_label", &game::scene::PartyMemberPanelViewModel::exp_label);
        handle.RegisterMember("hp_text", &game::scene::PartyMemberPanelViewModel::hp_text);
        handle.RegisterMember("mp_text", &game::scene::PartyMemberPanelViewModel::mp_text);
        handle.RegisterMember("portrait_decorator", &game::scene::PartyMemberPanelViewModel::portrait_decorator);
        handle.RegisterMember("selected", &game::scene::PartyMemberPanelViewModel::selected);
        handle.RegisterMember("empty", &game::scene::PartyMemberPanelViewModel::empty);
        handle.RegisterMember("targetable", &game::scene::PartyMemberPanelViewModel::targetable);
        return true;
    }
    return false;
}

[[nodiscard]] std::string_view tabShortcutTooltipText(int tab_index) {
    constexpr std::array<std::string_view, 5> labels{
        "Inventory [I]",
        "Equipment [C]",
        "Quests [J]",
        "Map [M]",
        "Options [O]",
    };

    if (tab_index < 0 || tab_index >= static_cast<int>(labels.size())) {
        return {};
    }
    return labels[static_cast<std::size_t>(tab_index)];
}

} // namespace

namespace game::scene {

InventoryMenuScene::InventoryMenuScene(std::string_view name,
                                       engine::core::Context& context,
                                       entt::registry& game_registry,
                                       entt::entity player,
                                       game::data::ItemCatalog* item_catalog,
                                       const game::data::RpgCatalog* rpg_catalog,
                                       const game::data::QuestCatalog* quest_catalog,
                                       const game::data::ShopCatalog* shop_catalog,
                                       const game::world::WorldState* world_state,
                                       game::runtime::UserSettingsService* user_settings_service,
                                       game::ui::MenuTabId initial_tab)
    : engine::scene::Scene(name, context),
      game_registry_(game_registry),
      player_(player),
      item_catalog_(item_catalog),
      rpg_catalog_(rpg_catalog),
      quest_catalog_(quest_catalog),
      shop_catalog_(shop_catalog),
      world_state_(world_state),
      user_settings_service_(user_settings_service),
      previous_state_(context.getGameState().getCurrentState()),
      initial_tab_id_(initial_tab) {}

InventoryMenuScene::~InventoryMenuScene() {
    disconnectRuntimeListeners();
    if (auto* tab = activeTab()) {
        tab->onDeactivated();
    }
    tabs_.clear();
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
    context_.getInputManager().onAction("inventory"_hs)
        .connect<&InventoryMenuScene::onInventoryTabShortcut>(this);
    context_.getInputManager().onAction("inventory_tab_equipment"_hs)
        .connect<&InventoryMenuScene::onEquipmentTabShortcut>(this);
    context_.getInputManager().onAction("inventory_tab_quests"_hs)
        .connect<&InventoryMenuScene::onQuestsTabShortcut>(this);
    context_.getInputManager().onAction("inventory_tab_map"_hs)
        .connect<&InventoryMenuScene::onMapTabShortcut>(this);
    context_.getInputManager().onAction("inventory_tab_options"_hs)
        .connect<&InventoryMenuScene::onOptionsTabShortcut>(this);

    return Scene::init();
}

void InventoryMenuScene::update(float delta_time) {
    if (auto* tab = activeTab()) {
        tab->update(delta_time);
    }
    updateTabShortcutTooltipPosition();
    Scene::update(delta_time);
}

void InventoryMenuScene::clean() {
    if (auto* tab = activeTab()) {
        tab->onDeactivated();
    }
    tabs_.clear();
    shutdownUI();
    disconnectRuntimeListeners();
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

    active_tab_id_ = initial_tab_id_;

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

        if (!constructor.RegisterArray<SlotGridViewModels>()) {
            spdlog::error("InventoryMenuScene: RegisterArray<SlotGridViewModels> 失败。");
            document_controller_.unload();
            return false;
        }

        if (!registerPartyMemberPanelViewModelType(constructor) ||
            !constructor.RegisterArray<PartyMemberPanelViewModels>()) {
            spdlog::error("InventoryMenuScene: PartyMemberPanelViewModel data types 注册失败。");
            document_controller_.unload();
            return false;
        }

        if (!game::ui::registerEquipmentTabDataTypes(constructor)) {
            spdlog::error("InventoryMenuScene: EquipmentTabContent data types 注册失败。");
            document_controller_.unload();
            return false;
        }

        if (!game::ui::registerQuestTabDataTypes(constructor)) {
            spdlog::error("InventoryMenuScene: QuestTabContent data types 注册失败。");
            document_controller_.unload();
            return false;
        }

        if (!game::ui::registerMapTabDataTypes(constructor)) {
            spdlog::error("InventoryMenuScene: MapTabContent data types 注册失败。");
            document_controller_.unload();
            return false;
        }

        data_types_registered_ = true;
    }

    constructor.Bind("party_members", &party_members_);
    constructor.Bind("gold_label", &gold_label_);
    constructor.Bind("farm_label", &farm_label_);

    if (!constructor.BindEventCallback(
            "switch_tab",
            [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& arguments) {
                switchTabFromTabsetIndex(game::ui::getSingleIntArgument(arguments));
            }) ||
        !constructor.BindEventCallback(
            "party_member_click",
            [this](Rml::DataModelHandle, Rml::Event& event, const Rml::VariantList& arguments) {
                event.StopPropagation();
                onPartyMemberClick(game::ui::getSingleIntArgument(arguments));
            }) ||
        !constructor.BindEventCallback(
            "tab_shortcut_hover",
            [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& arguments) {
                showTabShortcutTooltip(game::ui::getSingleIntArgument(arguments));
            }) ||
        !constructor.BindEventCallback(
            "tab_shortcut_exit",
            [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
                hideTabShortcutTooltip();
            })) {
        spdlog::error("InventoryMenuScene: 绑定全局 event 回调失败。");
        document_controller_.unload();
        return false;
    }

    tabs_.clear();
    equipment_tab_ = nullptr;
    auto inventory_tab = std::make_unique<game::ui::InventoryTabContent>(
        context_, document_controller_, game_registry_, player_, item_catalog_, instanceId());
    inventory_tab->setActorTargetRequestHandler([this](int inventory_slot_index) {
        beginActorTargetSelection(inventory_slot_index);
    });
    if (!inventory_tab->bindModel(constructor)) {
        spdlog::error("InventoryMenuScene: InventoryTabContent 绑定失败。");
        document_controller_.unload();
        return false;
    }
    tabs_.emplace(game::ui::MenuTabId::Inventory, std::move(inventory_tab));
    auto equipment_tab = std::make_unique<game::ui::EquipmentTabContent>(
        context_,
        document_controller_,
        game_registry_,
        player_,
        item_catalog_,
        rpg_catalog_,
        selected_actor_id_);
    equipment_tab_ = equipment_tab.get();
    if (!equipment_tab->bindModel(constructor)) {
        spdlog::error("InventoryMenuScene: EquipmentTabContent 绑定失败。");
        document_controller_.unload();
        return false;
    }
    tabs_.emplace(game::ui::MenuTabId::Equipment, std::move(equipment_tab));
    auto quest_tab = std::make_unique<game::ui::QuestTabContent>(
        document_controller_,
        game_registry_,
        player_,
        quest_catalog_);
    if (!quest_tab->bindModel(constructor)) {
        spdlog::error("InventoryMenuScene: QuestTabContent 绑定失败。");
        document_controller_.unload();
        return false;
    }
    tabs_.emplace(game::ui::MenuTabId::Quests, std::move(quest_tab));
    auto map_tab = std::make_unique<game::ui::MapTabContent>(
        document_controller_,
        game_registry_,
        player_,
        world_state_,
        quest_catalog_,
        shop_catalog_,
        rpg_catalog_,
        &runtime->generatedImages());
    if (!map_tab->bindModel(constructor)) {
        spdlog::error("InventoryMenuScene: MapTabContent 绑定失败。");
        document_controller_.unload();
        return false;
    }
    tabs_.emplace(game::ui::MenuTabId::Map, std::move(map_tab));
    auto options_tab = std::make_unique<game::ui::OptionsTabContent>(document_controller_, user_settings_service_);
    if (!options_tab->bindModel(constructor)) {
        spdlog::error("InventoryMenuScene: OptionsTabContent 绑定失败。");
        document_controller_.unload();
        return false;
    }
    tabs_.emplace(game::ui::MenuTabId::Options, std::move(options_tab));

    if (!document_controller_.load(DOCUMENT_PATH)) {
        tabs_.clear();
        document_controller_.unload();
        spdlog::error("InventoryMenuScene: 加载 RML 文档失败。");
        return false;
    }

    cacheTabShortcutTooltipElements();
    hideTabShortcutTooltip();
    syncPartyPanel();
    if (equipment_tab_) {
        equipment_tab_->setSelectedActor(selected_actor_id_);
    }
    if (!activateRmlTab(initial_tab_id_)) {
        spdlog::warn("InventoryMenuScene: 初始 tab 无法同步到 RmlUi tabset。");
    }
    if (auto* tab = activeTab()) {
        tab->onActivated();
    }
    document_controller_.markAllDirty();
    return true;
}

void InventoryMenuScene::shutdownUI() {
    hideTabShortcutTooltip();
    document_controller_.unload();
    tab_shortcut_tooltip_panel_ = nullptr;
    tab_shortcut_tooltip_text_ = nullptr;
}

void InventoryMenuScene::disconnectRuntimeListeners() {
    context_.getInputManager().onAction("menu_cancel"_hs)
        .disconnect<&InventoryMenuScene::onMenuCancelPressed>(this);
    context_.getInputManager().onAction("inventory"_hs)
        .disconnect<&InventoryMenuScene::onInventoryTabShortcut>(this);
    context_.getInputManager().onAction("inventory_tab_equipment"_hs)
        .disconnect<&InventoryMenuScene::onEquipmentTabShortcut>(this);
    context_.getInputManager().onAction("inventory_tab_quests"_hs)
        .disconnect<&InventoryMenuScene::onQuestsTabShortcut>(this);
    context_.getInputManager().onAction("inventory_tab_map"_hs)
        .disconnect<&InventoryMenuScene::onMapTabShortcut>(this);
    context_.getInputManager().onAction("inventory_tab_options"_hs)
        .disconnect<&InventoryMenuScene::onOptionsTabShortcut>(this);
}

void InventoryMenuScene::syncPartyPanel() {
    const InventoryMenuPartyPanelData data = buildInventoryMenuPartyPanelData(
        game_registry_,
        player_,
        rpg_catalog_,
        selected_actor_id_,
        actor_target_mode_);
    party_members_ = data.party_members;
    gold_label_ = data.gold_label;
    farm_label_ = data.farm_label;

    if (selected_actor_id_.empty()) {
        for (const auto& member : party_members_) {
            if (!member.empty) {
                selected_actor_id_ = member.actor_id;
                break;
            }
        }
    }

    document_controller_.markDirty("party_members");
    document_controller_.markDirty("gold_label");
    document_controller_.markDirty("farm_label");
    if (equipment_tab_) {
        equipment_tab_->setSelectedActor(selected_actor_id_);
    }
}

void InventoryMenuScene::beginActorTargetSelection(int inventory_slot_index) {
    if (inventory_slot_index < 0) {
        return;
    }
    actor_target_mode_ = true;
    pending_actor_target_inventory_slot_ = inventory_slot_index;
    syncPartyPanel();
}

void InventoryMenuScene::cancelActorTargetSelection() {
    if (!actor_target_mode_) {
        return;
    }
    actor_target_mode_ = false;
    pending_actor_target_inventory_slot_ = -1;
    syncPartyPanel();
}

void InventoryMenuScene::onPartyMemberClick(int party_slot_index) {
    if (party_slot_index < 0 || party_slot_index >= static_cast<int>(party_members_.size())) {
        return;
    }

    const auto& member = party_members_[static_cast<std::size_t>(party_slot_index)];
    if (member.empty || member.actor_id.empty()) {
        return;
    }

    selected_actor_id_ = member.actor_id;
    if (actor_target_mode_) {
        const int inventory_slot = pending_actor_target_inventory_slot_;
        actor_target_mode_ = false;
        pending_actor_target_inventory_slot_ = -1;
        if (inventory_slot >= 0) {
            context_.getDispatcher().trigger(game::defs::UseItemCommand{
                .target = player_,
                .inventory_slot_index = inventory_slot,
                .count = 1,
                .show_prompt = false,
                .actor_target_id = selected_actor_id_,
            });
        }
    }

    syncPartyPanel();
}

bool InventoryMenuScene::onMenuCancelPressed() {
    if (auto* tab = activeTab(); tab && tab->onCancel()) {
        return true;
    }

    if (actor_target_mode_) {
        cancelActorTargetSelection();
        return true;
    }

    requestPopScene();
    return true;
}

bool InventoryMenuScene::handleTabShortcut(game::ui::MenuTabId target_tab) {
    if (target_tab == active_tab_id_) {
        requestPopScene();
        return true;
    }

    if (!activateRmlTab(target_tab)) {
        spdlog::warn("InventoryMenuScene: 快捷键 tab 无法同步到 RmlUi tabset。");
    }
    return true;
}

bool InventoryMenuScene::onInventoryTabShortcut() {
    return handleTabShortcut(game::ui::MenuTabId::Inventory);
}

bool InventoryMenuScene::onEquipmentTabShortcut() {
    return handleTabShortcut(game::ui::MenuTabId::Equipment);
}

bool InventoryMenuScene::onQuestsTabShortcut() {
    return handleTabShortcut(game::ui::MenuTabId::Quests);
}

bool InventoryMenuScene::onMapTabShortcut() {
    return handleTabShortcut(game::ui::MenuTabId::Map);
}

bool InventoryMenuScene::onOptionsTabShortcut() {
    return handleTabShortcut(game::ui::MenuTabId::Options);
}

void InventoryMenuScene::switchTab(game::ui::MenuTabId new_tab) {
    const auto next_it = tabs_.find(new_tab);
    if (new_tab == active_tab_id_ || next_it == tabs_.end()) {
        return;
    }

    if (auto* tab = activeTab()) {
        tab->onDeactivated();
    }

    active_tab_id_ = new_tab;
    if (new_tab != game::ui::MenuTabId::Inventory) {
        cancelActorTargetSelection();
    }
    next_it->second->onActivated();
    if (new_tab == game::ui::MenuTabId::Equipment && equipment_tab_) {
        equipment_tab_->setSelectedActor(selected_actor_id_);
    }
}

void InventoryMenuScene::switchTabFromTabsetIndex(int tab_index) {
    if (const auto tab_id = toMenuTabId(tab_index)) {
        switchTab(*tab_id);
    }
}

bool InventoryMenuScene::activateRmlTab(game::ui::MenuTabId tab_id) {
    auto* document = document_controller_.document();
    if (!document) {
        spdlog::warn("InventoryMenuScene: cannot activate tab, RmlUi document is null.");
        return false;
    }

    auto* element = document->GetElementById("menu-tabset");
    auto* tabset = element ? rmlui_dynamic_cast<Rml::ElementTabSet*>(element) : nullptr;
    if (!tabset) {
        spdlog::warn("InventoryMenuScene: menu-tabset element is missing or not an ElementTabSet.");
        return false;
    }

    tabset->SetActiveTab(game::ui::tabsetIndexForMenuTab(tab_id));
    return true;
}

game::ui::IMenuTabContent* InventoryMenuScene::activeTab() {
    if (const auto it = tabs_.find(active_tab_id_); it != tabs_.end()) {
        return it->second.get();
    }
    return nullptr;
}

void InventoryMenuScene::cacheTabShortcutTooltipElements() {
    auto* document = document_controller_.document();
    tab_shortcut_tooltip_panel_ = document ? document->GetElementById("tab-shortcut-tooltip-panel") : nullptr;
    tab_shortcut_tooltip_text_ = document ? document->GetElementById("tab-shortcut-tooltip-text") : nullptr;
    if (!tab_shortcut_tooltip_panel_ || !tab_shortcut_tooltip_text_) {
        spdlog::warn("InventoryMenuScene: tab shortcut tooltip elements are missing.");
    }
}

void InventoryMenuScene::showTabShortcutTooltip(int tab_index) {
    if (!tab_shortcut_tooltip_panel_ || !tab_shortcut_tooltip_text_) {
        return;
    }

    const std::string_view label = tabShortcutTooltipText(tab_index);
    if (label.empty()) {
        hideTabShortcutTooltip();
        return;
    }

    tab_shortcut_tooltip_text_->SetInnerRML(textToInnerRml(label));
    tab_shortcut_tooltip_panel_->SetProperty("display", "inline-block");
    tab_shortcut_tooltip_visible_ = true;
    refreshTabShortcutTooltipMetrics();
    updateTabShortcutTooltipPosition();
}

void InventoryMenuScene::hideTabShortcutTooltip() {
    tab_shortcut_tooltip_visible_ = false;
    if (tab_shortcut_tooltip_panel_) {
        tab_shortcut_tooltip_panel_->SetProperty("display", "none");
    }
}

void InventoryMenuScene::refreshTabShortcutTooltipMetrics() {
    auto* document = document_controller_.document();
    if (!document || !tab_shortcut_tooltip_panel_) {
        return;
    }

    document->UpdateDocument();
    tab_shortcut_tooltip_size_ = {
        snapToPixel(tab_shortcut_tooltip_panel_->GetOffsetWidth()),
        snapToPixel(tab_shortcut_tooltip_panel_->GetOffsetHeight()),
    };
}

void InventoryMenuScene::updateTabShortcutTooltipPosition() {
    if (!tab_shortcut_tooltip_visible_ || !tab_shortcut_tooltip_panel_) {
        return;
    }

    const glm::vec2 mouse_pos = context_.getInputManager().getLogicalMousePosition();
    const glm::vec2 logical_size = context_.getGameState().getLogicalSize();

    glm::vec2 pos = mouse_pos + tab_shortcut_tooltip_offset_;
    if (pos.x + tab_shortcut_tooltip_size_.x > logical_size.x) {
        pos.x = mouse_pos.x - tab_shortcut_tooltip_offset_.x - tab_shortcut_tooltip_size_.x;
    }
    if (pos.y + tab_shortcut_tooltip_size_.y > logical_size.y) {
        pos.y = mouse_pos.y - tab_shortcut_tooltip_offset_.y - tab_shortcut_tooltip_size_.y;
    }

    pos.x = std::clamp(pos.x, 0.0F, std::max(0.0F, logical_size.x - tab_shortcut_tooltip_size_.x));
    pos.y = std::clamp(pos.y, 0.0F, std::max(0.0F, logical_size.y - tab_shortcut_tooltip_size_.y));

    setPixelProperty(tab_shortcut_tooltip_panel_, "left", pos.x);
    setPixelProperty(tab_shortcut_tooltip_panel_, "top", pos.y);
}

} // namespace game::scene
