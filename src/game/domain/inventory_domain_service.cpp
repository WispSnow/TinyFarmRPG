#include "inventory_domain_service.h"

#include "game/component/inventory_component.h"
#include "game/data/item_catalog.h"
#include "game/system/inventory_helpers.h"

#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>

#include <algorithm>

namespace game::domain {

using game::system::detail::tryFillEmpty;
using game::system::detail::tryMerge;

InventoryDomainService::InventoryDomainService(entt::registry& registry,
                                               entt::dispatcher& dispatcher,
                                               game::data::ItemCatalog& catalog)
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
                                         int active_page,
                                         bool from_add) const {
    game::defs::InventoryChanged evt{};
    evt.target = target;
    evt.slots = diff;
    evt.full_sync = false;
    evt.active_page = active_page;
    evt.from_add = from_add;
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
    if (!ensureInventory(target)) {
        result.rejected = count;
        return result;
    }

    auto& inv = registry_.get<game::component::InventoryComponent>(target);
    const auto* item = catalog_.findItem(item_id);
    const int stack_limit = item ? std::max(1, item->stack_limit_) : 999;
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
        emitChanged(target, diff, inv.active_page_, true);
    }

    result.changed_slots = diff;
    result.active_page = inv.active_page_;
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
        emitChanged(target, diff, inv.active_page_, false);
    }

    result.changed_slots = diff;
    result.active_page = inv.active_page_;
    result.accepted = count - remaining;
    result.rejected = remaining;
    return result;
}

} // namespace game::domain
