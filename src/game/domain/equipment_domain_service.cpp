#include "game/domain/equipment_domain_service.h"

#include "game/component/inventory_component.h"
#include "game/component/party_component.h"
#include "game/component/party_equipment_component.h"
#include "game/data/item_catalog.h"
#include "game/data/rpg_catalog.h"
#include "game/defs/events.h"
#include "game/domain/inventory_domain_service.h"
#include "game/system/inventory_helpers.h"

#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
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

} // namespace

EquipmentDomainService::EquipmentDomainService(entt::registry& registry,
                                               entt::dispatcher& dispatcher,
                                               const game::data::RpgCatalog& rpg_catalog,
                                               const game::data::ItemCatalog& item_catalog,
                                               InventoryDomainService& inventory_domain_service)
    : registry_(registry),
      dispatcher_(dispatcher),
      rpg_catalog_(rpg_catalog),
      item_catalog_(item_catalog),
      inventory_domain_service_(inventory_domain_service) {}

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

    auto& equipment_component = registry_.get_or_emplace<game::component::PartyEquipmentComponent>(player);
    auto& loadout = equipment_component.loadouts_by_actor_id_[actor_id];
    const auto equipped_it = loadout.equipped_item_ids_.find(target_slot);
    const entt::id_type old_item_id =
        equipped_it == loadout.equipped_item_ids_.end() ? entt::null : equipped_it->second;

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

    const auto remove_result =
        inventory_domain_service_.removeItem(player, source_stack.item_id_, 1, inventory_slot_index);
    if (remove_result.accepted != 1 || remove_result.rejected != 0) {
        spdlog::error(
            "EquipmentDomainService: simulated equip removal failed during commit actor='{}' item_id={} slot={}",
            actor_id,
            static_cast<std::uint64_t>(source_stack.item_id_),
            inventory_slot_index);
        return {.message = "failed to remove equipment item"};
    }

    loadout.equipped_item_ids_[target_slot] = source_stack.item_id_;
    ++equipment_component.revision_;

    if (old_item_id != entt::null) {
        const auto add_result =
            inventory_domain_service_.addItem(player, old_item_id, 1, inventory_slot_index);
        if (add_result.accepted != 1 || add_result.rejected != 0) {
            spdlog::error(
                "EquipmentDomainService: simulated replaced-equipment return failed during commit actor='{}' old_item_id={}",
                actor_id,
                static_cast<std::uint64_t>(old_item_id));
            return {.message = "failed to return replaced equipment"};
        }
    }

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
    const auto& inventory = registry_.get<game::component::InventoryComponent>(player);
    if (preferred_inventory_slot < -1 || preferred_inventory_slot >= inventory.slotCount()) {
        return {.message = "invalid preferred inventory slot"};
    }
    std::vector<game::component::ItemStack> simulated = inventory.slots_;
    if (!game::system::detail::simulateAdd(
            simulated,
            preferred_inventory_slot,
            item_id,
            1,
            stackLimitOrDefault(item_catalog_, item_id))) {
        return {.message = "inventory is full"};
    }

    const auto add_result = inventory_domain_service_.addItem(player, item_id, 1, preferred_inventory_slot);
    if (add_result.accepted != 1 || add_result.rejected != 0) {
        spdlog::error(
            "EquipmentDomainService: simulated unequip return failed during commit actor='{}' item_id={}",
            actor_id,
            static_cast<std::uint64_t>(item_id));
        return {.message = "failed to return equipment item"};
    }

    loadout.equipped_item_ids_.erase(equipped_it);
    ++equipment_component->revision_;
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
