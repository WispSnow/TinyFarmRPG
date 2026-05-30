#include "game/domain/equipment_domain_service.h"

#include "game/component/inventory_component.h"
#include "game/component/party_component.h"
#include "game/component/party_equipment_component.h"
#include "game/data/item_catalog.h"
#include "game/data/rpg_catalog.h"
#include "game/defs/events.h"
#include "game/system/inventory_helpers.h"

#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace game::domain {
namespace {

[[nodiscard]] bool containsString(const std::vector<std::string>& values, const std::string_view value) {
    return std::any_of(values.begin(), values.end(), [value](const std::string& current) {
        return current == value;
    });
}

[[nodiscard]] bool classAllowed(const game::data::EquipmentData& equipment, const std::string_view class_id) {
    return equipment.allowed_classes_.empty() || containsString(equipment.allowed_classes_, class_id);
}

[[nodiscard]] bool actorAllowed(const game::data::EquipmentData& equipment, const std::string_view actor_id) {
    return equipment.allowed_actors_.empty() || containsString(equipment.allowed_actors_, actor_id);
}

[[nodiscard]] int stackLimitOrDefault(const game::data::ItemCatalog& catalog, const entt::id_type item_id) {
    return game::system::detail::stackLimitOrDefault(&catalog, item_id);
}

[[nodiscard]] std::vector<game::defs::InventorySlotUpdate>
collectChangedSlots(const std::vector<game::component::ItemStack>& before,
                    const std::vector<game::component::ItemStack>& after) {
    std::vector<game::defs::InventorySlotUpdate> updates{};
    updates.reserve(after.size());
    for (std::size_t i = 0; i < after.size(); ++i) {
        if (before[i].item_id_ == after[i].item_id_ && before[i].count_ == after[i].count_) {
            continue;
        }
        updates.push_back(game::defs::InventorySlotUpdate{
            .slot_index = static_cast<int>(i),
            .item_id = after[i].item_id_,
            .count = after[i].count_,
        });
    }
    return updates;
}

void triggerInventoryChanged(entt::dispatcher& dispatcher,
                             const entt::entity player,
                             const std::vector<game::defs::InventorySlotUpdate>& diff) {
    if (diff.empty()) {
        return;
    }

    game::defs::InventoryChanged evt{};
    evt.target = player;
    evt.slots = diff;
    dispatcher.trigger(evt);
}

} // namespace

EquipmentDomainService::EquipmentDomainService(entt::registry& registry,
                                               entt::dispatcher& dispatcher,
                                               const game::data::RpgCatalog& rpg_catalog,
                                               const game::data::ItemCatalog& item_catalog)
    : registry_(registry),
      dispatcher_(dispatcher),
      rpg_catalog_(rpg_catalog),
      item_catalog_(item_catalog) {}

EquipmentMutationResult EquipmentDomainService::equipItem(entt::entity player,
                                                          const std::string& actor_id,
                                                          int inventory_slot_index,
                                                          game::data::EquipmentSlotId target_slot) {
    if (player == entt::null || actor_id.empty() || target_slot == game::data::EquipmentSlotId::Unknown) {
        return {.message = "invalid equip request"};
    }
    if (!registry_.valid(player) ||
        !registry_.all_of<game::component::InventoryComponent, game::component::PartyComponent>(player)) {
        return {.message = "player missing inventory or party"};
    }

    const auto& party = registry_.get<game::component::PartyComponent>(player);
    if (!containsString(party.recruited_actor_ids_, actor_id)) {
        return {.message = "actor is not recruited"};
    }

    auto& inventory = registry_.get<game::component::InventoryComponent>(player);
    if (inventory_slot_index < 0 || inventory_slot_index >= inventory.slotCount()) {
        return {.message = "invalid inventory slot"};
    }

    const auto source_stack = inventory.slot(inventory_slot_index);
    if (source_stack.empty()) {
        return {.message = "inventory slot is empty"};
    }
    if (source_stack.count_ != 1) {
        return {.message = "equipment stack count must be 1"};
    }

    const auto* equipment = rpg_catalog_.findEquipmentByItem(source_stack.item_id_);
    if (!equipment || equipment->slot_ != target_slot) {
        return {.message = "item is not valid equipment for this slot"};
    }

    const auto* actor = rpg_catalog_.findActor(actor_id);
    if (!actor || !classAllowed(*equipment, actor->class_id_) || !actorAllowed(*equipment, actor_id)) {
        return {.message = "actor cannot equip this item"};
    }

    const auto* equipment_component = registry_.try_get<game::component::PartyEquipmentComponent>(player);
    const game::component::ActorEquipmentLoadout* current_loadout = nullptr;
    if (equipment_component) {
        const auto loadout_it = equipment_component->loadouts_by_actor_id_.find(actor_id);
        if (loadout_it != equipment_component->loadouts_by_actor_id_.end()) {
            current_loadout = &loadout_it->second;
        }
    }

    entt::id_type old_item_id = entt::null;
    if (current_loadout) {
        const auto equipped_it = current_loadout->equipped_item_ids_.find(target_slot);
        if (equipped_it != current_loadout->equipped_item_ids_.end()) {
            old_item_id = equipped_it->second;
        }
    }
    if (old_item_id != entt::null && item_catalog_.findItem(old_item_id) == nullptr) {
        return {.message = "equipped item is unknown"};
    }

    const auto before_slots = inventory.slots_;
    std::vector<game::component::ItemStack> simulated = inventory.slots_;
    auto& simulated_source = simulated[static_cast<std::size_t>(inventory_slot_index)];
    if (simulated_source.item_id_ != source_stack.item_id_ || simulated_source.count_ <= 0) {
        return {.message = "inventory changed before equip"};
    }
    --simulated_source.count_;
    if (simulated_source.count_ <= 0) {
        simulated_source.clear();
    }

    if (old_item_id != entt::null &&
        !game::system::detail::simulateAdd(
            simulated,
            inventory_slot_index,
            old_item_id,
            1,
            stackLimitOrDefault(item_catalog_, old_item_id))) {
        return {.message = "inventory is full"};
    }

    inventory.slots_ = std::move(simulated);
    auto& mutable_equipment = registry_.get_or_emplace<game::component::PartyEquipmentComponent>(player);
    auto& mutable_loadout = mutable_equipment.loadouts_by_actor_id_[actor_id];
    mutable_loadout.equipped_item_ids_[target_slot] = source_stack.item_id_;
    ++mutable_equipment.revision_;

    triggerInventoryChanged(dispatcher_, player, collectChangedSlots(before_slots, inventory.slots_));
    emitChanged(player, actor_id, target_slot, source_stack.item_id_);
    return {.success = true};
}

EquipmentMutationResult EquipmentDomainService::unequipItem(entt::entity player,
                                                            const std::string& actor_id,
                                                            game::data::EquipmentSlotId slot,
                                                            int preferred_inventory_slot) {
    if (player == entt::null || actor_id.empty() || slot == game::data::EquipmentSlotId::Unknown) {
        return {.message = "invalid unequip request"};
    }
    if (!registry_.valid(player) ||
        !registry_.all_of<game::component::InventoryComponent, game::component::PartyComponent>(player)) {
        return {.message = "player missing inventory or party"};
    }

    const auto& party = registry_.get<game::component::PartyComponent>(player);
    if (!containsString(party.recruited_actor_ids_, actor_id)) {
        return {.message = "actor is not recruited"};
    }

    auto* equipment_component = registry_.try_get<game::component::PartyEquipmentComponent>(player);
    if (!equipment_component) {
        return {.message = "no equipment component"};
    }

    auto loadout_it = equipment_component->loadouts_by_actor_id_.find(actor_id);
    if (loadout_it == equipment_component->loadouts_by_actor_id_.end()) {
        return {.message = "actor has no equipment"};
    }

    auto& loadout = loadout_it->second;
    const auto equipped_it = loadout.equipped_item_ids_.find(slot);
    if (equipped_it == loadout.equipped_item_ids_.end() || equipped_it->second == entt::null) {
        return {.message = "slot is empty"};
    }

    const entt::id_type item_id = equipped_it->second;
    if (item_catalog_.findItem(item_id) == nullptr) {
        return {.message = "equipped item is unknown"};
    }

    const auto& inventory = registry_.get<game::component::InventoryComponent>(player);
    if (preferred_inventory_slot < -1 || preferred_inventory_slot >= inventory.slotCount()) {
        return {.message = "invalid preferred inventory slot"};
    }
    const auto before_slots = inventory.slots_;
    std::vector<game::component::ItemStack> simulated = inventory.slots_;
    if (!game::system::detail::simulateAdd(
            simulated,
            preferred_inventory_slot,
            item_id,
            1,
            stackLimitOrDefault(item_catalog_, item_id))) {
        return {.message = "inventory is full"};
    }

    auto& mutable_inventory = registry_.get<game::component::InventoryComponent>(player);
    mutable_inventory.slots_ = std::move(simulated);
    loadout.equipped_item_ids_.erase(equipped_it);
    ++equipment_component->revision_;

    triggerInventoryChanged(dispatcher_, player, collectChangedSlots(before_slots, mutable_inventory.slots_));
    emitChanged(player, actor_id, slot, entt::null);
    return {.success = true};
}

void EquipmentDomainService::emitChanged(entt::entity player,
                                         const std::string& actor_id,
                                         game::data::EquipmentSlotId slot,
                                         entt::id_type item_id) const {
    game::defs::EquipmentChanged evt{};
    evt.player = player;
    evt.actor_id = actor_id;
    evt.full_sync = false;
    evt.slots.push_back(game::defs::EquipmentSlotUpdate{
        .actor_id = actor_id,
        .slot = slot,
        .item_id = item_id,
    });
    dispatcher_.trigger(evt);
}

} // namespace game::domain
