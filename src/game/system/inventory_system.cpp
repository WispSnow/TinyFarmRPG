#include "inventory_system.h"
#include "game/component/inventory_component.h"
#include "game/domain/inventory_domain_service.h"

#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>

namespace game::system {

namespace {

[[nodiscard]] std::vector<game::defs::InventorySlotUpdate>
collectInventoryUpdates(const game::component::InventoryComponent& inventory) {
    std::vector<game::defs::InventorySlotUpdate> updates;
    updates.reserve(static_cast<std::size_t>(inventory.slotCount()));
    for (int i = 0; i < inventory.slotCount(); ++i) {
        const auto& stack = inventory.slot(i);
        updates.push_back({i, stack.item_id_, stack.count_});
    }
    return updates;
}

} // namespace

InventorySystem::InventorySystem(entt::registry& registry,
                                 entt::dispatcher& dispatcher,
                                 game::domain::InventoryDomainService& inventory_domain_service)
    : registry_(registry),
      dispatcher_(dispatcher),
      inventory_domain_service_(inventory_domain_service) {
    subscribe();
}

InventorySystem::~InventorySystem() {
    unsubscribe();
}

void InventorySystem::subscribe() {
    dispatcher_.sink<game::defs::AddItemCommand>().connect<&InventorySystem::onAddItem>(this);
    dispatcher_.sink<game::defs::RemoveItemCommand>().connect<&InventorySystem::onRemoveItem>(this);
    dispatcher_.sink<game::defs::InventorySyncCommand>().connect<&InventorySystem::onSync>(this);
    dispatcher_.sink<game::defs::InventoryMoveCommand>().connect<&InventorySystem::onMoveItem>(this);
    dispatcher_.sink<game::defs::InventorySortCommand>().connect<&InventorySystem::onSort>(this);
}

void InventorySystem::unsubscribe() {
    dispatcher_.sink<game::defs::AddItemCommand>().disconnect<&InventorySystem::onAddItem>(this);
    dispatcher_.sink<game::defs::RemoveItemCommand>().disconnect<&InventorySystem::onRemoveItem>(this);
    dispatcher_.sink<game::defs::InventorySyncCommand>().disconnect<&InventorySystem::onSync>(this);
    dispatcher_.sink<game::defs::InventoryMoveCommand>().disconnect<&InventorySystem::onMoveItem>(this);
    dispatcher_.sink<game::defs::InventorySortCommand>().disconnect<&InventorySystem::onSort>(this);
}

void InventorySystem::emitChanged(entt::entity target,
                                  const std::vector<game::defs::InventorySlotUpdate>& diff,
                                  bool full_sync,
                                  game::defs::InventoryMoveKind move_kind,
                                  int move_from_slot,
                                  int move_to_slot) {
    game::defs::InventoryChanged evt{};
    evt.target = target;
    evt.slots = diff;
    evt.full_sync = full_sync;
    evt.from_add = false;
    evt.move_kind = move_kind;
    evt.move_from_slot = move_from_slot;
    evt.move_to_slot = move_to_slot;
    dispatcher_.trigger(evt);
}

void InventorySystem::onAddItem(const game::defs::AddItemCommand& evt) {
    (void)inventory_domain_service_.addItem(
        evt.target, evt.item_id, evt.count, evt.preferred_slot_index);
}

void InventorySystem::onRemoveItem(const game::defs::RemoveItemCommand& evt) {
    (void)inventory_domain_service_.removeItem(
        evt.target, evt.item_id, evt.count, evt.slot_index);
}

void InventorySystem::onSync(const game::defs::InventorySyncCommand& evt) {
    if (evt.target == entt::null) return;
    if (!registry_.valid(evt.target) || !registry_.all_of<game::component::InventoryComponent>(evt.target)) return;
    auto& inv = registry_.get<game::component::InventoryComponent>(evt.target);
    emitChanged(evt.target, collectInventoryUpdates(inv), true);
}

void InventorySystem::onMoveItem(const game::defs::InventoryMoveCommand& evt) {
    (void)inventory_domain_service_.moveItem(evt.target, evt.from_slot, evt.to_slot, evt.allow_merge);
}

void InventorySystem::onSort(const game::defs::InventorySortCommand& evt) {
    (void)inventory_domain_service_.sortInventory(evt.target);
}

} // namespace game::system
