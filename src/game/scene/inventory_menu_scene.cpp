#include "inventory_menu_scene.h"

#include "engine/core/context.h"
#include "engine/core/game_state.h"
#include "engine/input/input_manager.h"
#include "engine/resource/decoded_image.h"
#include "engine/resource/image_decode_service.h"
#include "engine/ui/rmlui/rml_element_helpers.h"
#include "engine/ui/rmlui/rml_generated_image_registry.h"
#include "engine/ui/rmlui/rml_ui_texture_filter_mode.h"
#include "engine/ui/rmlui/rml_ui_runtime.h"
#include "game/component/party_runtime_stats_component.h"
#include "game/data/item_catalog.h"
#include "game/data/quest_catalog.h"
#include "game/data/rpg_catalog.h"
#include "game/data/rpg_data.h"
#include "game/data/shop_catalog.h"
#include "game/defs/commands.h"
#include "game/defs/events_inventory.h"
#include "game/defs/events_party.h"
#include "game/defs/options_events.h"
#include "game/defs/party_ids.h"
#include "game/runtime/user_settings_service.h"
#include "game/scene/inventory_menu_character_panel.h"
#include "game/ui/equipment_tab_content.h"
#include "game/ui/inventory_menu_tab_shortcuts.h"
#include "game/ui/inventory_tab_content.h"
#include "game/ui/localized_text.h"
#include "game/ui/map_tab_content.h"
#include "game/ui/options_tab_content.h"
#include "game/ui/player_portrait_service.h"
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
#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace entt::literals;

namespace {

constexpr std::string_view DOCUMENT_PATH = "ui/rmlui/scenes/inventory_menu.rml";
constexpr std::string_view MODEL_NAME = "inventory_menu";
constexpr float HINT_FEEDBACK_SECONDS = 2.0F; ///< 使用结果反馈的停留时长。
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
        handle.RegisterMember("portrait_src", &game::scene::PartyMemberPanelViewModel::portrait_src);
        handle.RegisterMember("selected", &game::scene::PartyMemberPanelViewModel::selected);
        handle.RegisterMember("empty", &game::scene::PartyMemberPanelViewModel::empty);
        handle.RegisterMember("targetable", &game::scene::PartyMemberPanelViewModel::targetable);
        handle.RegisterMember("has_portrait", &game::scene::PartyMemberPanelViewModel::has_portrait);
        return true;
    }
    return false;
}

[[nodiscard]] engine::resource::DecodedImage cropPortraitImage(
    const engine::resource::DecodedImage& source,
    const game::data::PortraitRefData& portrait) {
    if (!source.valid() || source.channels != 4 || !portrait.valid() ||
        portrait.x_ < 0 || portrait.y_ < 0 ||
        portrait.x_ + portrait.width_ > source.width ||
        portrait.y_ + portrait.height_ > source.height) {
        return {};
    }

    engine::resource::DecodedImage output{};
    output.width = portrait.width_;
    output.height = portrait.height_;
    output.channels = 4;
    const std::size_t row_bytes = static_cast<std::size_t>(output.width) * 4U;
    output.pixels.resize(row_bytes * static_cast<std::size_t>(output.height));

    for (int y = 0; y < output.height; ++y) {
        const std::size_t source_offset =
            (static_cast<std::size_t>(portrait.y_ + y) * static_cast<std::size_t>(source.width) +
             static_cast<std::size_t>(portrait.x_)) * 4U;
        const std::size_t target_offset = static_cast<std::size_t>(y) * row_bytes;
        std::copy_n(source.pixels.data() + source_offset, row_bytes, output.pixels.data() + target_offset);
    }

    return output;
}

[[nodiscard]] std::string partyPortraitSourceUri(
    std::uint64_t scene_instance_id,
    int slot_index,
    const game::data::ActorData& actor) {
    return fmt::format(
        "generated://inventory-party-portrait/{}/{}/{}",
        scene_instance_id,
        slot_index,
        actor.id_hash_);
}

[[nodiscard]] std::string_view tabShortcutTooltipText(int tab_index) {
    constexpr std::array<std::string_view, 5> labels{
        "Inventory I",
        "Equipment C",
        "Quests J",
        "Map M",
        "Options O",
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
    context_.getDispatcher().sink<game::defs::InventoryChanged>()
        .connect<&InventoryMenuScene::onInventoryChanged>(this);
    context_.getDispatcher().sink<game::defs::ItemUsedEvent>()
        .connect<&InventoryMenuScene::onItemUsed>(this);
    context_.getDispatcher().sink<game::defs::PartyRuntimeStatsChanged>()
        .connect<&InventoryMenuScene::onPartyRuntimeStatsChanged>(this);
    context_.getDispatcher().sink<game::defs::LanguageChangedEvent>()
        .connect<&InventoryMenuScene::onLanguageChanged>(this);

    return Scene::init();
}

void InventoryMenuScene::update(float delta_time) {
    if (auto* tab = activeTab()) {
        tab->update(delta_time);
    }
    updatePartyHint(delta_time);
    updateTabShortcutTooltipPosition();
    Scene::update(delta_time);
}

void InventoryMenuScene::prepareUi(float interpolation_alpha) {
    Scene::prepareUi(interpolation_alpha);
    flushPendingUiRefreshes();
    if (auto* tab = activeTab()) {
        tab->prepareUi();
    }
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
    constructor.Bind("inventory_capacity_label", &inventory_capacity_label_);
    constructor.Bind("party_hint_text", &party_hint_text_);
    constructor.Bind("party_hint_visible", &party_hint_visible_);

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
    clearPartyPortraitImages();
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
    context_.getDispatcher().sink<game::defs::InventoryChanged>()
        .disconnect<&InventoryMenuScene::onInventoryChanged>(this);
    context_.getDispatcher().sink<game::defs::ItemUsedEvent>()
        .disconnect<&InventoryMenuScene::onItemUsed>(this);
    context_.getDispatcher().sink<game::defs::PartyRuntimeStatsChanged>()
        .disconnect<&InventoryMenuScene::onPartyRuntimeStatsChanged>(this);
    context_.getDispatcher().sink<game::defs::LanguageChangedEvent>()
        .disconnect<&InventoryMenuScene::onLanguageChanged>(this);
}

void InventoryMenuScene::clearPartyPortraitImages() {
    party_portrait_registrations_.clear();
}

void InventoryMenuScene::registerPartyPortraitImages(std::vector<PartyMemberPanelViewModel>& members) {
    clearPartyPortraitImages();

    auto* runtime = context_.getRmlUi();
    if (!runtime || !rpg_catalog_) {
        return;
    }

    auto& generated_images = runtime->generatedImages();
    party_portrait_registrations_.reserve(members.size());
    game::ui::PlayerPortraitService* player_portrait_service = nullptr;
    if (auto** service_ptr = game_registry_.ctx().find<game::ui::PlayerPortraitService*>()) {
        player_portrait_service = *service_ptr;
    }
    for (auto& member : members) {
        member.portrait_src.clear();
        member.has_portrait = false;

        if (member.empty || member.actor_id.empty()) {
            continue;
        }

        if (member.actor_id == game::defs::kDefaultPlayerActorId && player_portrait_service &&
            player_portrait_service->ready()) {
            member.portrait_src = std::string(
                player_portrait_service->sourceUri(game::ui::PortraitImageKind::Standard64Linear));
            member.has_portrait = !member.portrait_src.empty();
            continue;
        }

        const auto* actor = rpg_catalog_->findActor(member.actor_id);
        if (!actor || !actor->portrait_.valid()) {
            continue;
        }

        auto decoded = engine::resource::ImageDecodeService::decodeRGBA(actor->portrait_.path_);
        if (!decoded) {
            spdlog::warn(
                "InventoryMenuScene: 无法解码角色头像 '{}'。",
                actor->portrait_.path_);
            continue;
        }

        auto portrait_image = cropPortraitImage(*decoded, actor->portrait_);
        if (!portrait_image.valid()) {
            spdlog::warn(
                "InventoryMenuScene: 角色 '{}' 的头像裁剪区域非法。",
                actor->id_);
            continue;
        }

        const std::string source_uri = partyPortraitSourceUri(instanceId(), member.slot_index, *actor);
        auto registration = generated_images.registerImage(
            source_uri,
            std::move(portrait_image),
            engine::ui::rmlui::RmlUiTextureFilterMode::Linear);
        if (!registration.valid()) {
            continue;
        }

        member.portrait_src = source_uri;
        member.has_portrait = true;
        party_portrait_registrations_.push_back(std::move(registration));
    }
}

void InventoryMenuScene::syncPartyPanel() {
    InventoryMenuPartyPanelData data = buildInventoryMenuPartyPanelData(
        game_registry_,
        player_,
        rpg_catalog_,
        selected_actor_id_,
        actor_target_mode_,
        localization());
    registerPartyPortraitImages(data.party_members);
    party_members_ = data.party_members;
    gold_label_ = data.gold_label;
    inventory_capacity_label_ = data.inventory_capacity_label;

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
    document_controller_.markDirty("inventory_capacity_label");
    if (equipment_tab_) {
        equipment_tab_->setSelectedActor(selected_actor_id_);
    }
}

void InventoryMenuScene::refreshInventoryCapacityLabel() {
    inventory_capacity_label_ = buildInventoryMenuCapacityLabel(game_registry_, player_, localization());
    document_controller_.markDirty("inventory_capacity_label");
}

void InventoryMenuScene::flushPendingUiRefreshes() {
    if (std::exchange(party_panel_refresh_pending_, false)) {
        inventory_capacity_refresh_pending_ = false;
        syncPartyPanel();
        return;
    }
    if (std::exchange(inventory_capacity_refresh_pending_, false)) {
        refreshInventoryCapacityLabel();
    }
}

void InventoryMenuScene::beginActorTargetSelection(int inventory_slot_index) {
    if (inventory_slot_index < 0) {
        return;
    }
    actor_target_mode_ = true;
    pending_actor_target_inventory_slot_ = inventory_slot_index;
    party_panel_refresh_pending_ = true;
    // 选目标是一个独立步骤：没有这条提示，玩家会以为点了“使用”就已经用掉道具。
    showPartyHint(game::ui::localizeTextOrFallback(
        localization(), "inventory.item_use.select_target", "Select a character to use it on"));
}

void InventoryMenuScene::cancelActorTargetSelection() {
    if (!actor_target_mode_) {
        return;
    }
    actor_target_mode_ = false;
    pending_actor_target_inventory_slot_ = -1;
    party_panel_refresh_pending_ = true;
    hidePartyHint();
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
            // UseItemCommand 是同步派发的：命令返回后 onItemUsed 已经跑完，
            // 这里可以直接用前后 HP 差值组织反馈文案。
            const auto hp_before = findActorCurrentHp(selected_actor_id_);
            item_used_this_click_ = false;
            context_.getDispatcher().trigger(game::defs::UseItemCommand{
                .target = player_,
                .inventory_slot_index = inventory_slot,
                .count = 1,
                .show_prompt = false,
                .actor_target_id = selected_actor_id_,
            });

            if (!item_used_this_click_) {
                hidePartyHint();
            } else {
                const auto hp_after = findActorCurrentHp(selected_actor_id_);
                const int hp_gain = (hp_before && hp_after) ? (*hp_after - *hp_before) : 0;
                showTimedPartyHint(
                    hp_gain > 0
                        ? game::ui::formatTextOrFallback(
                              localization(),
                              "item.use.recovered_hp",
                              {{"amount", std::to_string(hp_gain)}},
                              [hp_gain] { return "Recovered " + std::to_string(hp_gain) + " HP"; })
                        : game::ui::localizeTextOrFallback(localization(), "item.use.used", "Used"),
                    HINT_FEEDBACK_SECONDS);
            }
        } else {
            hidePartyHint();
        }
    }

    party_panel_refresh_pending_ = true;
}

std::optional<int> InventoryMenuScene::findActorCurrentHp(std::string_view actor_id) const {
    if (actor_id.empty() || player_ == entt::null || !game_registry_.valid(player_)) {
        return std::nullopt;
    }
    const auto* runtime = game_registry_.try_get<game::component::PartyRuntimeStatsComponent>(player_);
    if (!runtime) {
        return std::nullopt;
    }
    const auto it = runtime->states_by_actor_id_.find(std::string{actor_id});
    if (it == runtime->states_by_actor_id_.end()) {
        return std::nullopt;
    }
    return it->second.current_hp;
}

void InventoryMenuScene::showPartyHint(std::string_view text) {
    party_hint_text_ = Rml::String{text.data(), text.size()};
    party_hint_visible_ = true;
    party_hint_seconds_left_ = 0.0F;
    document_controller_.markDirty("party_hint_text");
    document_controller_.markDirty("party_hint_visible");
}

void InventoryMenuScene::showTimedPartyHint(std::string_view text, float seconds) {
    showPartyHint(text);
    party_hint_seconds_left_ = std::max(0.0F, seconds);
}

void InventoryMenuScene::hidePartyHint() {
    if (!party_hint_visible_ && party_hint_text_.empty()) {
        return;
    }
    party_hint_text_.clear();
    party_hint_visible_ = false;
    party_hint_seconds_left_ = 0.0F;
    document_controller_.markDirty("party_hint_text");
    document_controller_.markDirty("party_hint_visible");
}

void InventoryMenuScene::updatePartyHint(float delta_time) {
    if (party_hint_seconds_left_ <= 0.0F) {
        return;
    }
    party_hint_seconds_left_ -= delta_time;
    if (party_hint_seconds_left_ <= 0.0F) {
        hidePartyHint();
    }
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

const game::runtime::LocalizationService* InventoryMenuScene::localization() const noexcept {
    return user_settings_service_ ? &user_settings_service_->localization() : nullptr;
}

void InventoryMenuScene::onLanguageChanged(const game::defs::LanguageChangedEvent& /*event*/) {
    party_panel_refresh_pending_ = true;
    if (auto* tab = activeTab()) {
        tab->onLanguageChanged();
    }
    hideTabShortcutTooltip();
}

void InventoryMenuScene::onInventoryChanged(const game::defs::InventoryChanged& event) {
    if (event.target != player_) {
        return;
    }

    inventory_capacity_refresh_pending_ = true;
}

void InventoryMenuScene::onItemUsed(const game::defs::ItemUsedEvent& event) {
    if (event.target != player_) {
        return;
    }

    item_used_this_click_ = true;
}

void InventoryMenuScene::onPartyRuntimeStatsChanged(const game::defs::PartyRuntimeStatsChanged& event) {
    if (event.player != player_) {
        return;
    }

    party_panel_refresh_pending_ = true;
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
