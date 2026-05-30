#include "inventory_domain_service.h"

#include "game/component/inventory_component.h"
#include "game/data/item_catalog.h"
#include "game/system/inventory_helpers.h"

#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>

#include <algorithm>
#include <string_view>

namespace game::domain {

using game::system::detail::tryFillEmpty;
using game::system::detail::tryMerge;

namespace {

[[nodiscard]] int categoryOrder(const game::data::ItemCategory category) {
    using game::data::ItemCategory;
    switch (category) {
        case ItemCategory::Tool: return 0;
        case ItemCategory::Seed: return 1;
        case ItemCategory::Consumable: return 2;
        case ItemCategory::Crop: return 3;
        case ItemCategory::Material: return 4;
        case ItemCategory::Equipment: return 5;
        case ItemCategory::Unknown:
        default:
            return 6;
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

InventoryDomainService::InventoryDomainService(entt::registry& registry,
                                               entt::dispatcher& dispatcher,
                                               const game::data::ItemCatalog& catalog)
    : registry_(registry), dispatcher_(dispatcher), catalog_(catalog) {
}

bool InventoryDomainService::ensureInventory(entt::entity target) {
    if (!registry_.valid(target)) return false;
    if (!registry_.all_of<game::component::InventoryComponent>(target)) {
        registry_.emplace<game::component::InventoryComponent>(target);
    }
    return true;
}

void InventoryDomainService::emitChanged(entt::entity target,
                                         const std::vector<game::defs::InventorySlotUpdate>& diff,
                                         bool from_add,
                                         bool full_sync,
                                         game::defs::InventoryMoveKind move_kind,
                                         int move_from_slot,
                                         int move_to_slot) const {
    game::defs::InventoryChanged evt{};
    evt.target = target;
    evt.slots = diff;
    evt.full_sync = full_sync;
    evt.from_add = from_add;
    evt.move_kind = move_kind;
    evt.move_from_slot = move_from_slot;
    evt.move_to_slot = move_to_slot;
    dispatcher_.trigger(evt);
}

InventoryMutationResult InventoryDomainService::addItem(entt::entity target,
                                                        entt::id_type item_id,
                                                        int count,
                                                        int preferred_slot_index) {
    InventoryMutationResult result{};
    result.target = target;
    if (target == entt::null || item_id == entt::null || count <= 0) {
        return result;
    }
    const auto* item = catalog_.findItem(item_id);
    if (item == nullptr) {
        result.rejected = count;
        return result;
    }
    if (!ensureInventory(target)) {
        result.rejected = count;
        return result;
    }

    auto& inv = registry_.get<game::component::InventoryComponent>(target);
    const int stack_limit = std::max(1, item->stack_limit_);
    int remaining = count;
    std::vector<game::defs::InventorySlotUpdate> diff;
    diff.reserve(4);

    if (preferred_slot_index >= 0 && preferred_slot_index < inv.slotCount() && remaining > 0) {
        auto& stack = inv.slot(preferred_slot_index);
        const entt::id_type before_id = stack.item_id_;
        const int before_count = stack.count_;
        remaining = tryMerge(stack, item_id, remaining, stack_limit);
        remaining = tryFillEmpty(stack, item_id, remaining, stack_limit);
        if (stack.item_id_ != before_id || stack.count_ != before_count) {
            diff.push_back({preferred_slot_index, stack.item_id_, stack.count_});
        }
    }

    for (int i = 0; i < inv.slotCount() && remaining > 0; ++i) {
        auto& stack = inv.slot(i);
        const int before_count = stack.count_;
        remaining = tryMerge(stack, item_id, remaining, stack_limit);
        if (stack.count_ != before_count) {
            diff.push_back({i, stack.item_id_, stack.count_});
        }
    }

    for (int i = 0; i < inv.slotCount() && remaining > 0; ++i) {
        auto& stack = inv.slot(i);
        const int before_count = stack.count_;
        remaining = tryFillEmpty(stack, item_id, remaining, stack_limit);
        if (stack.count_ != before_count) {
            diff.push_back({i, stack.item_id_, stack.count_});
        }
    }

    if (!diff.empty()) {
        emitChanged(target, diff, true);
    }

    result.changed_slots = diff;
    result.accepted = count - remaining;
    result.rejected = remaining;

    if (remaining > 0) {
        game::defs::InventoryFullEvent full_evt{};
        full_evt.target = target;
        full_evt.item_id = item_id;
        full_evt.rejected = remaining;
        dispatcher_.trigger(full_evt);
    }

    return result;
}

InventoryMutationResult InventoryDomainService::removeItem(entt::entity target,
                                                           entt::id_type item_id,
                                                           int count,
                                                           int slot_index) {
    InventoryMutationResult result{};
    result.target = target;
    if (target == entt::null || item_id == entt::null || count <= 0) {
        return result;
    }
    if (!registry_.valid(target) || !registry_.all_of<game::component::InventoryComponent>(target)) {
        result.rejected = count;
        return result;
    }

    auto& inv = registry_.get<game::component::InventoryComponent>(target);
    int remaining = count;
    std::vector<game::defs::InventorySlotUpdate> diff;
    diff.reserve(4);

    auto removeFromSlot = [&](int index) {
        if (index < 0 || index >= inv.slotCount() || remaining <= 0) return;
        auto& stack = inv.slot(index);
        if (stack.empty() || stack.item_id_ != item_id) return;
        const int take = std::min(stack.count_, remaining);
        stack.count_ -= take;
        remaining -= take;
        if (stack.count_ <= 0) {
            stack.clear();
        }
        diff.push_back({index, stack.item_id_, stack.count_});
    };

    if (slot_index >= 0) {
        removeFromSlot(slot_index);
    } else {
        for (int i = 0; i < inv.slotCount() && remaining > 0; ++i) {
            removeFromSlot(i);
        }
    }

    if (!diff.empty()) {
        emitChanged(target, diff, false);
    }

    result.changed_slots = diff;
    result.accepted = count - remaining;
    result.rejected = remaining;
    return result;
}

bool InventoryDomainService::moveItem(const entt::entity target,
                                      const int from_slot,
                                      const int to_slot,
                                      const bool allow_merge) {
    if (target == entt::null) return false;
    if (!registry_.valid(target) || !registry_.all_of<game::component::InventoryComponent>(target)) return false;

    auto& inv = registry_.get<game::component::InventoryComponent>(target);
    if (from_slot < 0 || from_slot >= inv.slotCount()) return false;
    if (to_slot < 0 || to_slot >= inv.slotCount()) return false;
    if (from_slot == to_slot) return false;

    auto& from = inv.slot(from_slot);
    auto& to = inv.slot(to_slot);
    if (from.empty()) return false;

    const int stack_limit = game::system::detail::stackLimitOrDefault(&catalog_, from.item_id_);
    std::vector<game::defs::InventorySlotUpdate> diff;
    diff.reserve(2);
    game::defs::InventoryMoveKind move_kind = game::defs::InventoryMoveKind::None;

    if (allow_merge && !to.empty() && to.item_id_ == from.item_id_ && to.count_ < stack_limit) {
        move_kind = game::defs::InventoryMoveKind::Merge;
        const int space = stack_limit - to.count_;
        const int moved = std::min(space, from.count_);
        to.count_ += moved;
        from.count_ -= moved;
        if (from.count_ <= 0) {
            from.clear();
        }
        diff.push_back({from_slot, from.item_id_, from.count_});
        diff.push_back({to_slot, to.item_id_, to.count_});
    } else if (to.empty()) {
        move_kind = game::defs::InventoryMoveKind::MoveToEmpty;
        to = from;
        from.clear();
        diff.push_back({from_slot, from.item_id_, from.count_});
        diff.push_back({to_slot, to.item_id_, to.count_});
    } else {
        move_kind = game::defs::InventoryMoveKind::Swap;
        std::swap(from, to);
        diff.push_back({from_slot, from.item_id_, from.count_});
        diff.push_back({to_slot, to.item_id_, to.count_});
    }

    emitChanged(target, diff, false, false, move_kind, from_slot, to_slot);
    return true;
}

bool InventoryDomainService::sortInventory(const entt::entity target) {
    if (target == entt::null) return false;
    if (!registry_.valid(target) || !registry_.all_of<game::component::InventoryComponent>(target)) return false;

    auto& inventory = registry_.get<game::component::InventoryComponent>(target);

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

        const std::string_view lhs_id = lhs_item ? std::string_view{lhs_item->id_str_} : std::string_view{};
        const std::string_view rhs_id = rhs_item ? std::string_view{rhs_item->id_str_} : std::string_view{};
        if (lhs_id != rhs_id) {
            return lhs_id < rhs_id;
        }

        return lhs.original_index < rhs.original_index;
    });

    std::vector<game::component::ItemStack> sorted_slots(static_cast<std::size_t>(inventory.slotCount()));
    std::vector<int> old_to_new(static_cast<std::size_t>(inventory.slotCount()), -1);
    for (int new_index = 0; new_index < inventory.slotCount(); ++new_index) {
        const auto& entry = entries[static_cast<std::size_t>(new_index)];
        sorted_slots[static_cast<std::size_t>(new_index)] = entry.stack;
        if (!entry.stack.empty()) {
            old_to_new[static_cast<std::size_t>(entry.original_index)] = new_index;
        }
    }
    inventory.slots_ = std::move(sorted_slots);

    game::defs::InventoryChanged evt{};
    evt.target = target;
    evt.slots = collectInventoryUpdates(inventory);
    evt.slot_remap_old_to_new = std::move(old_to_new);
    evt.full_sync = true;
    dispatcher_.trigger(evt);
    return true;
}

} // namespace game::domain
