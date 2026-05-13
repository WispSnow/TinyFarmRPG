#include "game/ui/equipment_tab_content.h"

#include "engine/core/context.h"
#include "game/battle/actor_stats_resolver.h"
#include "game/component/inventory_component.h"
#include "game/component/party_component.h"
#include "game/component/party_equipment_component.h"
#include "game/data/item_catalog.h"
#include "game/data/rpg_catalog.h"
#include "game/data/rpg_data.h"
#include "game/defs/commands.h"
#include "game/defs/events.h"
#include "game/ui/rml_item_icon_helpers.h"
#include "game/ui/slot_grid_support.h"

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/DataTypeRegister.h>
#include <RmlUi/Core/Event.h>
#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>
#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace game::ui {
namespace {

using EquipmentSlotViewModels = std::vector<EquipmentSlotViewModel>;
using EquipmentCandidateViewModels = std::vector<EquipmentCandidateViewModel>;

struct EquipmentSlotDef {
    game::data::EquipmentSlotId id{game::data::EquipmentSlotId::Unknown};
    std::string_view label{};
    std::string_view placeholder_decorator{};
};

constexpr std::array<EquipmentSlotDef, 5> kEquipmentSlots{{
    {game::data::EquipmentSlotId::Weapon, "Weapon", "image(equipment-slot-weapon-hint)"},
    {game::data::EquipmentSlotId::Head, "Head", "image(equipment-slot-head-hint)"},
    {game::data::EquipmentSlotId::Body, "Body", "image(equipment-slot-body-hint)"},
    {game::data::EquipmentSlotId::Boot, "Boot", "image(equipment-slot-boot-hint)"},
    {game::data::EquipmentSlotId::Accessory, "Accessory", "image(equipment-slot-accessory-hint)"},
}};

constexpr std::array<std::string_view, game::data::kParamCount> kParamLabels{{
    "HP",
    "MP",
    "ATK",
    "DEF",
    "MAT",
    "MDF",
    "AGI",
    "LUK",
}};

[[nodiscard]] const EquipmentSlotDef* findSlotDef(game::data::EquipmentSlotId slot) {
    const auto it = std::find_if(kEquipmentSlots.begin(), kEquipmentSlots.end(), [slot](const EquipmentSlotDef& def) {
        return def.id == slot;
    });
    return it == kEquipmentSlots.end() ? nullptr : &*it;
}

[[nodiscard]] std::optional<game::data::EquipmentSlotId> slotFromIndex(int slot_index) {
    if (slot_index < 0 || slot_index >= static_cast<int>(kEquipmentSlots.size())) {
        return std::nullopt;
    }
    return kEquipmentSlots[static_cast<std::size_t>(slot_index)].id;
}

[[nodiscard]] bool containsString(const std::vector<std::string>& values, const std::string_view value) {
    return std::any_of(values.begin(), values.end(), [value](const std::string& current) {
        return current == value;
    });
}

[[nodiscard]] const game::component::ActorEquipmentLoadout* findLoadout(const entt::registry& registry,
                                                                        entt::entity player,
                                                                        std::string_view actor_id) {
    const auto* equipment = registry.try_get<game::component::PartyEquipmentComponent>(player);
    if (!equipment) {
        return nullptr;
    }
    const auto it = equipment->loadouts_by_actor_id_.find(std::string{actor_id});
    return it == equipment->loadouts_by_actor_id_.end() ? nullptr : &it->second;
}

[[nodiscard]] entt::id_type findEquippedItem(const entt::registry& registry,
                                             entt::entity player,
                                             std::string_view actor_id,
                                             game::data::EquipmentSlotId slot) {
    const auto* loadout = findLoadout(registry, player, actor_id);
    if (!loadout) {
        return entt::null;
    }
    const auto it = loadout->equipped_item_ids_.find(slot);
    return it == loadout->equipped_item_ids_.end() ? entt::null : it->second;
}

[[nodiscard]] bool actorCanEquip(const game::data::RpgCatalog* catalog,
                                 const game::data::EquipmentData& equipment,
                                 std::string_view actor_id) {
    const auto* actor = catalog ? catalog->findActor(actor_id) : nullptr;
    if (!actor) {
        return false;
    }
    if (!equipment.allowed_classes_.empty() && !containsString(equipment.allowed_classes_, actor->class_id_)) {
        return false;
    }
    return equipment.allowed_actors_.empty() || containsString(equipment.allowed_actors_, actor_id);
}

[[nodiscard]] std::string makeParamDeltaText(const game::data::EquipmentData& candidate,
                                             const game::data::EquipmentData* equipped) {
    std::string text{};
    for (std::size_t i = 0; i < game::data::kParamCount; ++i) {
        const int current = equipped ? equipped->param_bonuses_[i] : 0;
        const int delta = candidate.param_bonuses_[i] - current;
        if (delta == 0) {
            continue;
        }
        if (!text.empty()) {
            text.append("  ");
        }
        text += fmt::format("{}{} {}", delta > 0 ? "+" : "", delta, kParamLabels[i]);
    }
    return text;
}

[[nodiscard]] std::string makeActorSummary(const game::data::RpgCatalog* catalog,
                                           const game::data::ActorData* actor,
                                           const game::component::ActorEquipmentLoadout* loadout) {
    if (!catalog || !actor) {
        return {};
    }

    const auto* klass = catalog->findClass(actor->class_id_);
    const auto resolved = game::battle::resolveActorStats(*catalog, *actor, loadout);
    const auto hp = resolved.params[static_cast<std::size_t>(game::data::ParamIndex::Mhp)];
    const auto mp = resolved.params[static_cast<std::size_t>(game::data::ParamIndex::Mmp)];
    const auto atk = resolved.params[static_cast<std::size_t>(game::data::ParamIndex::Atk)];
    const auto def = resolved.params[static_cast<std::size_t>(game::data::ParamIndex::Def)];

    return fmt::format(
        "{} Lv.{}  HP {}  MP {}  ATK {}  DEF {}",
        klass && !klass->display_name_.empty() ? klass->display_name_ : "Adventurer",
        actor->initial_level_,
        hp,
        mp,
        atk,
        def);
}

[[nodiscard]] bool registerEquipmentSlotViewModelType(Rml::DataModelConstructor& constructor) {
    if (auto handle = constructor.RegisterStruct<EquipmentSlotViewModel>()) {
        handle.RegisterMember("slot_index", &EquipmentSlotViewModel::slot_index);
        handle.RegisterMember("label", &EquipmentSlotViewModel::label);
        handle.RegisterMember("item_name", &EquipmentSlotViewModel::item_name);
        handle.RegisterMember("placeholder_decorator", &EquipmentSlotViewModel::placeholder_decorator);
        handle.RegisterMember("icon_decorator", &EquipmentSlotViewModel::icon_decorator);
        handle.RegisterMember("has_item", &EquipmentSlotViewModel::has_item);
        handle.RegisterMember("is_selected", &EquipmentSlotViewModel::is_selected);
        return true;
    }
    return false;
}

[[nodiscard]] bool registerEquipmentCandidateViewModelType(Rml::DataModelConstructor& constructor) {
    if (auto handle = constructor.RegisterStruct<EquipmentCandidateViewModel>()) {
        handle.RegisterMember("inventory_slot_index", &EquipmentCandidateViewModel::inventory_slot_index);
        handle.RegisterMember("item_name", &EquipmentCandidateViewModel::item_name);
        handle.RegisterMember("category_label", &EquipmentCandidateViewModel::category_label);
        handle.RegisterMember("icon_decorator", &EquipmentCandidateViewModel::icon_decorator);
        handle.RegisterMember("delta_text", &EquipmentCandidateViewModel::delta_text);
        handle.RegisterMember("has_delta", &EquipmentCandidateViewModel::has_delta);
        return true;
    }
    return false;
}

} // namespace

bool registerEquipmentTabDataTypes(Rml::DataModelConstructor& constructor) {
    if (!registerEquipmentSlotViewModelType(constructor) ||
        !registerEquipmentCandidateViewModelType(constructor)) {
        return false;
    }
    return constructor.RegisterArray<EquipmentSlotViewModels>() &&
           constructor.RegisterArray<EquipmentCandidateViewModels>();
}

EquipmentTabContent::EquipmentTabContent(engine::core::Context& context,
                                         engine::ui::rmlui::RmlDocumentController& document_controller,
                                         entt::registry& game_registry,
                                         entt::entity player,
                                         game::data::ItemCatalog* item_catalog,
                                         const game::data::RpgCatalog* rpg_catalog,
                                         std::string_view selected_actor_id)
    : context_(context),
      document_controller_(document_controller),
      game_registry_(game_registry),
      player_(player),
      item_catalog_(item_catalog),
      rpg_catalog_(rpg_catalog),
      selected_actor_id_(selected_actor_id) {
    equipment_slots_.resize(kEquipmentSlots.size());
}

EquipmentTabContent::~EquipmentTabContent() {
    disconnectRuntimeListeners();
}

bool EquipmentTabContent::bindModel(Rml::DataModelConstructor& constructor) {
    if (!constructor.Bind("equipment_slots", &equipment_slots_) ||
        !constructor.Bind("equipment_candidates", &equipment_candidates_) ||
        !constructor.Bind("equipment_actor_name", &equipment_actor_name_) ||
        !constructor.Bind("equipment_actor_summary", &equipment_actor_summary_) ||
        !constructor.Bind("equipment_detail_name", &equipment_detail_name_) ||
        !constructor.Bind("equipment_detail_category", &equipment_detail_category_) ||
        !constructor.Bind("equipment_detail_description", &equipment_detail_description_) ||
        !constructor.Bind("has_equipment_candidates", &has_equipment_candidates_) ||
        !constructor.Bind("has_equipment_detail", &has_equipment_detail_)) {
        spdlog::error("EquipmentTabContent: 绑定 equipment 页 data model 失败。");
        return false;
    }

    if (!bindIndexedEventCallback(
            constructor,
            "equipment_slot_click",
            this,
            &EquipmentTabContent::onEquipmentSlotClick) ||
        !bindIndexedEventCallback(
            constructor,
            "equipment_candidate_click",
            this,
            &EquipmentTabContent::onEquipmentCandidateClick) ||
        !document_controller_.bindSimpleEvent(constructor, "equipment_unequip", [this] { onUnequipClicked(); })) {
        spdlog::error("EquipmentTabContent: 绑定 equipment 页 event 回调失败。");
        return false;
    }

    return true;
}

void EquipmentTabContent::onActivated() {
    connectRuntimeListeners();
    syncViewState();
}

void EquipmentTabContent::onDeactivated() {
    disconnectRuntimeListeners();
}

void EquipmentTabContent::update(float /*delta_time*/) {
}

bool EquipmentTabContent::onCancel() {
    return false;
}

void EquipmentTabContent::setSelectedActor(std::string_view actor_id) {
    selected_actor_id_ = actor_id;
    syncViewState();
}

void EquipmentTabContent::connectRuntimeListeners() {
    if (listeners_connected_) {
        return;
    }
    context_.getDispatcher().sink<game::defs::InventoryChanged>()
        .connect<&EquipmentTabContent::onInventoryChanged>(this);
    context_.getDispatcher().sink<game::defs::EquipmentChanged>()
        .connect<&EquipmentTabContent::onEquipmentChanged>(this);
    listeners_connected_ = true;
}

void EquipmentTabContent::disconnectRuntimeListeners() {
    if (!listeners_connected_) {
        return;
    }
    context_.getDispatcher().sink<game::defs::InventoryChanged>()
        .disconnect<&EquipmentTabContent::onInventoryChanged>(this);
    context_.getDispatcher().sink<game::defs::EquipmentChanged>()
        .disconnect<&EquipmentTabContent::onEquipmentChanged>(this);
    listeners_connected_ = false;
}

void EquipmentTabContent::syncViewState() {
    syncActorHeader();
    syncSlots();
    syncCandidates();
    syncDetail();
    markEquipmentDirty();
}

void EquipmentTabContent::syncActorHeader() {
    const auto* actor = rpg_catalog_ ? rpg_catalog_->findActor(selected_actor_id_) : nullptr;
    equipment_actor_name_ = actor && !actor->display_name_.empty() ? actor->display_name_ : selected_actor_id_;
    equipment_actor_summary_ = makeActorSummary(
        rpg_catalog_,
        actor,
        findLoadout(game_registry_, player_, selected_actor_id_));
}

void EquipmentTabContent::syncSlots() {
    for (std::size_t i = 0; i < kEquipmentSlots.size(); ++i) {
        const auto& def = kEquipmentSlots[i];
        auto& vm = equipment_slots_[i];
        vm.slot_index = static_cast<int>(i);
        vm.label = Rml::String{def.label.data(), def.label.size()};
        vm.item_name = "Empty";
        vm.placeholder_decorator = Rml::String{def.placeholder_decorator.data(), def.placeholder_decorator.size()};
        vm.icon_decorator = std::string{kNoDecorator};
        vm.has_item = false;
        vm.is_selected = (selected_slot_ == def.id);

        const entt::id_type item_id = findEquippedItem(game_registry_, player_, selected_actor_id_, def.id);
        const auto* item = item_catalog_ ? item_catalog_->findItem(item_id) : nullptr;
        if (!item) {
            continue;
        }

        vm.item_name = item->display_name_;
        vm.icon_decorator = buildItemIconDecorator(item_catalog_, item_id);
        vm.has_item = hasDecorator(vm.icon_decorator);
    }
}

void EquipmentTabContent::syncCandidates() {
    equipment_candidates_.clear();
    const auto* inventory = game_registry_.try_get<game::component::InventoryComponent>(player_);
    const auto* equipped = rpg_catalog_ ? rpg_catalog_->findEquipmentByItem(equippedItemForSelectedSlot()) : nullptr;
    if (!inventory || !rpg_catalog_ || selected_actor_id_.empty()) {
        has_equipment_candidates_ = false;
        return;
    }

    for (int slot_index = 0; slot_index < inventory->slotCount(); ++slot_index) {
        const auto& stack = inventory->slot(slot_index);
        if (stack.empty()) {
            continue;
        }

        const auto* equipment = rpg_catalog_->findEquipmentByItem(stack.item_id_);
        if (!equipment || equipment->slot_ != selected_slot_ || !actorCanEquip(rpg_catalog_, *equipment, selected_actor_id_)) {
            continue;
        }

        const auto* item = item_catalog_ ? item_catalog_->findItem(stack.item_id_) : nullptr;
        if (!item) {
            continue;
        }

        const std::string delta = makeParamDeltaText(*equipment, equipped);
        equipment_candidates_.push_back(EquipmentCandidateViewModel{
            .inventory_slot_index = slot_index,
            .item_name = item->display_name_,
            .category_label = item->category_str_,
            .icon_decorator = buildItemIconDecorator(item_catalog_, stack.item_id_),
            .delta_text = delta,
            .has_delta = !delta.empty(),
        });
    }

    has_equipment_candidates_ = !equipment_candidates_.empty();
}

void EquipmentTabContent::syncDetail() {
    const auto* def = findSlotDef(selected_slot_);
    const entt::id_type item_id = equippedItemForSelectedSlot();
    const auto* item = item_catalog_ ? item_catalog_->findItem(item_id) : nullptr;

    if (!item) {
        equipment_detail_name_ = def ? Rml::String{def->label.data(), def->label.size()} : "Equipment";
        equipment_detail_category_ = "Slot";
        equipment_detail_description_ = "No equipment equipped.";
        has_equipment_detail_ = true;
        return;
    }

    equipment_detail_name_ = item->display_name_;
    equipment_detail_category_ = item->category_str_;
    equipment_detail_description_ = item->description_;
    has_equipment_detail_ = true;
}

void EquipmentTabContent::markEquipmentDirty() {
    document_controller_.markDirty("equipment_slots");
    document_controller_.markDirty("equipment_candidates");
    document_controller_.markDirty("equipment_actor_name");
    document_controller_.markDirty("equipment_actor_summary");
    document_controller_.markDirty("equipment_detail_name");
    document_controller_.markDirty("equipment_detail_category");
    document_controller_.markDirty("equipment_detail_description");
    document_controller_.markDirty("has_equipment_candidates");
    document_controller_.markDirty("has_equipment_detail");
}

entt::id_type EquipmentTabContent::equippedItemForSelectedSlot() const {
    return findEquippedItem(game_registry_, player_, selected_actor_id_, selected_slot_);
}

void EquipmentTabContent::selectSlotByIndex(int slot_index) {
    const auto slot = slotFromIndex(slot_index);
    if (!slot || *slot == selected_slot_) {
        return;
    }

    selected_slot_ = *slot;
    syncViewState();
}

void EquipmentTabContent::equipCandidateFromInventorySlot(int inventory_slot_index) {
    if (player_ == entt::null || selected_actor_id_.empty() || inventory_slot_index < 0) {
        return;
    }

    context_.getDispatcher().trigger(game::defs::EquipItemCommand{
        .player = player_,
        .actor_id = selected_actor_id_,
        .inventory_slot_index = inventory_slot_index,
        .target_slot = selected_slot_,
    });
}

void EquipmentTabContent::unequipSelectedSlot() {
    if (player_ == entt::null || selected_actor_id_.empty() || equippedItemForSelectedSlot() == entt::null) {
        return;
    }

    context_.getDispatcher().trigger(game::defs::UnequipItemCommand{
        .player = player_,
        .actor_id = selected_actor_id_,
        .slot = selected_slot_,
        .preferred_inventory_slot = -1,
    });
}

void EquipmentTabContent::onEquipmentSlotClick(int slot_index, Rml::Event& event) {
    event.StopPropagation();
    selectSlotByIndex(slot_index);
}

void EquipmentTabContent::onEquipmentCandidateClick(int inventory_slot_index, Rml::Event& event) {
    event.StopPropagation();
    equipCandidateFromInventorySlot(inventory_slot_index);
}

void EquipmentTabContent::onUnequipClicked() {
    unequipSelectedSlot();
}

void EquipmentTabContent::onInventoryChanged(const game::defs::InventoryChanged& evt) {
    if (evt.target != player_) {
        return;
    }
    syncViewState();
}

void EquipmentTabContent::onEquipmentChanged(const game::defs::EquipmentChanged& evt) {
    if (evt.player != player_) {
        return;
    }
    if (!evt.actor_id.empty() && evt.actor_id != selected_actor_id_) {
        return;
    }
    syncViewState();
}

} // namespace game::ui
