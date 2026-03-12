#include "inventory_system.h"
#include "game/component/hotbar_component.h"
#include "game/component/inventory_component.h"
#include "game/data/item_catalog.h"
#include "game/domain/inventory_domain_service.h"

#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>

#include <algorithm>
#include <string_view>

namespace game::system {

namespace {

[[nodiscard]] int categoryOrder(game::data::ItemCategory category) {
    using game::data::ItemCategory;
    switch (category) {
        case ItemCategory::Tool: return 0;
        case ItemCategory::Seed: return 1;
        case ItemCategory::Consumable: return 2;
        case ItemCategory::Crop: return 3;
        case ItemCategory::Material: return 4;
        case ItemCategory::Unknown:
        default:
            return 5;
    }
}

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
                                 game::data::ItemCatalog& catalog,
                                 game::domain::InventoryDomainService& inventory_domain_service)
    : registry_(registry),
      dispatcher_(dispatcher),
      catalog_(catalog),
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

bool InventorySystem::ensureInventory(entt::entity target) {
    if (!registry_.valid(target)) return false;
    if (!registry_.all_of<game::component::InventoryComponent>(target)) {
        registry_.emplace<game::component::InventoryComponent>(target);
    }
    return true;
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
    if (evt.target == entt::null) return;
    if (!registry_.valid(evt.target) || !registry_.all_of<game::component::InventoryComponent>(evt.target)) return;

    auto& inv = registry_.get<game::component::InventoryComponent>(evt.target);
    if (evt.from_slot < 0 || evt.from_slot >= inv.slotCount()) return;
    if (evt.to_slot < 0 || evt.to_slot >= inv.slotCount()) return;
    if (evt.from_slot == evt.to_slot) return;

    auto& from = inv.slot(evt.from_slot);
    auto& to = inv.slot(evt.to_slot);
    if (from.empty()) return;

    const auto* item = catalog_.findItem(from.item_id_);
    const int stack_limit = item ? item->stack_limit_ : 999;

    std::vector<game::defs::InventorySlotUpdate> diff;
    diff.reserve(2);
    game::defs::InventoryMoveKind move_kind = game::defs::InventoryMoveKind::None;

    if (evt.allow_merge && !to.empty() && to.item_id_ == from.item_id_ && to.count_ < stack_limit) {
        move_kind = game::defs::InventoryMoveKind::Merge;
        const int space = stack_limit - to.count_;
        const int moved = std::min(space, from.count_);
        to.count_ += moved;
        from.count_ -= moved;
        if (from.count_ <= 0) {
            from.clear();
        }
        diff.push_back({evt.from_slot, from.item_id_, from.count_});
        diff.push_back({evt.to_slot, to.item_id_, to.count_});
    } else if (to.empty()) {
        move_kind = game::defs::InventoryMoveKind::MoveToEmpty;
        to = from;
        from.clear();
        diff.push_back({evt.from_slot, from.item_id_, from.count_});
        diff.push_back({evt.to_slot, to.item_id_, to.count_});
    } else {
        move_kind = game::defs::InventoryMoveKind::Swap;
        std::swap(from, to);
        diff.push_back({evt.from_slot, from.item_id_, from.count_});
        diff.push_back({evt.to_slot, to.item_id_, to.count_});
    }

    if (!diff.empty()) {
        emitChanged(evt.target, diff, false, move_kind, evt.from_slot, evt.to_slot);
    }
}

void InventorySystem::onSort(const game::defs::InventorySortCommand& evt) {
    if (evt.target == entt::null) return;
    if (!registry_.valid(evt.target) || !registry_.all_of<game::component::InventoryComponent>(evt.target)) return;

    auto& inventory = registry_.get<game::component::InventoryComponent>(evt.target);

    struct SortEntry {
        game::component::ItemStack stack{};
        int original_index{-1};
    };

    std::vector<SortEntry> entries;
    entries.reserve(static_cast<std::size_t>(inventory.slotCount()));
    for (int i = 0; i < inventory.slotCount(); ++i) {
        entries.push_back({inventory.slot(i), i});
    }

    std::stable_sort(entries.begin(), entries.end(), [this](const SortEntry& lhs, const SortEntry& rhs) {
        const bool lhs_empty = lhs.stack.empty();
        const bool rhs_empty = rhs.stack.empty();
        if (lhs_empty != rhs_empty) {
            return !lhs_empty;
        }
        if (lhs_empty) {
            return lhs.original_index < rhs.original_index;
        }

        const auto* lhs_item = catalog_.findItem(lhs.stack.item_id_);
        const auto* rhs_item = catalog_.findItem(rhs.stack.item_id_);
        const int lhs_category = categoryOrder(lhs_item ? lhs_item->category_ : game::data::ItemCategory::Unknown);
        const int rhs_category = categoryOrder(rhs_item ? rhs_item->category_ : game::data::ItemCategory::Unknown);
        if (lhs_category != rhs_category) {
            return lhs_category < rhs_category;
        }

        const std::string_view lhs_name = lhs_item ? std::string_view{lhs_item->display_name_} : std::string_view{};
        const std::string_view rhs_name = rhs_item ? std::string_view{rhs_item->display_name_} : std::string_view{};
        if (lhs_name != rhs_name) {
            return lhs_name < rhs_name;
        }

        return lhs.original_index < rhs.original_index;
    });

    std::vector<game::component::ItemStack> sorted_slots(static_cast<std::size_t>(inventory.slotCount()));
    std::vector<int> old_to_new(static_cast<std::size_t>(inventory.slotCount()), -1);
    for (int new_index = 0; new_index < inventory.slotCount(); ++new_index) {
        sorted_slots[static_cast<std::size_t>(new_index)] = entries[static_cast<std::size_t>(new_index)].stack;
        if (!entries[static_cast<std::size_t>(new_index)].stack.empty()) {
            old_to_new[static_cast<std::size_t>(entries[static_cast<std::size_t>(new_index)].original_index)] = new_index;
        }
    }
    inventory.slots_ = std::move(sorted_slots);

    if (auto* hotbar = registry_.try_get<game::component::HotbarComponent>(evt.target)) {
        for (int i = 0; i < game::component::HotbarComponent::SLOT_COUNT; ++i) {
            auto& slot = hotbar->slot(i);
            const int old_inventory_slot = slot.inventory_slot_index_;
            if (old_inventory_slot < 0 || old_inventory_slot >= inventory.slotCount()) {
                slot.clear();
                continue;
            }

            const int new_inventory_slot = old_to_new[static_cast<std::size_t>(old_inventory_slot)];
            if (new_inventory_slot < 0 || new_inventory_slot >= inventory.slotCount() || inventory.slot(new_inventory_slot).empty()) {
                slot.clear();
                continue;
            }
            slot.inventory_slot_index_ = new_inventory_slot;
        }
    }

    emitChanged(evt.target, collectInventoryUpdates(inventory), true);
    if (registry_.all_of<game::component::HotbarComponent>(evt.target)) {
        dispatcher_.trigger(game::defs::HotbarSyncCommand{evt.target, true});
    }
}

} // namespace game::system
