#include "hotbar_system.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <optional>

namespace game::system {

namespace {

[[nodiscard]] bool isItemOnHotbar(const game::component::HotbarComponent& hotbar,
                                  const game::component::InventoryComponent& inventory,
                                  entt::id_type item_id) {
    for (int i = 0; i < game::component::HotbarComponent::SLOT_COUNT; ++i) {
        const int inv_index = hotbar.slot(i).inventory_slot_index_;
        if (inv_index < 0 || inv_index >= inventory.slotCount()) continue;
        const auto& stack = inventory.slot(inv_index);
        if (!stack.empty() && stack.item_id_ == item_id) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] std::optional<int> findHotbarSlotToFill(const game::component::HotbarComponent& hotbar,
                                                      const game::component::InventoryComponent& inventory) {
    for (int i = 0; i < game::component::HotbarComponent::SLOT_COUNT; ++i) {
        if (hotbar.slot(i).empty()) {
            return i;
        }
    }

    for (int i = 0; i < game::component::HotbarComponent::SLOT_COUNT; ++i) {
        const int inv_index = hotbar.slot(i).inventory_slot_index_;
        if (inv_index < 0 || inv_index >= inventory.slotCount()) {
            return i;
        }
        if (inventory.slot(inv_index).empty()) {
            return i;
        }
    }

    return std::nullopt;
}

[[nodiscard]] bool inventorySlotAffected(const game::defs::InventoryChanged& evt, int slot_index) {
    if (evt.full_sync) return true;
    return std::any_of(evt.slots.begin(), evt.slots.end(), [slot_index](const auto& update) {
        return update.slot_index == slot_index;
    });
}

[[nodiscard]] int selectInventorySlotForItem(const std::vector<game::defs::InventorySlotUpdate>& diff,
                                             const game::component::InventoryComponent& inventory,
                                             entt::id_type item_id) {
    int best = -1;
    for (const auto& update : diff) {
        if (update.item_id != item_id || update.count <= 0) continue;
        best = best < 0 ? update.slot_index : std::min(best, update.slot_index);
    }
    if (best >= 0) return best;

    for (int i = 0; i < inventory.slotCount(); ++i) {
        const auto& stack = inventory.slot(i);
        if (!stack.empty() && stack.item_id_ == item_id) {
            return i;
        }
    }
    return -1;
}

void upsertUpdate(std::vector<game::defs::HotbarSlotUpdate>& updates,
                  game::defs::HotbarSlotUpdate update) {
    const auto it = std::find_if(updates.begin(), updates.end(), [idx = update.hotbar_index](const auto& current) {
        return current.hotbar_index == idx;
    });
    if (it == updates.end()) {
        updates.push_back(update);
    } else {
        *it = update;
    }
}

void pushSlotUpdate(std::vector<game::defs::HotbarSlotUpdate>& updates,
                    const game::component::InventoryComponent& inventory,
                    int hotbar_index,
                    int inventory_slot_index) {
    game::defs::HotbarSlotUpdate update{};
    update.hotbar_index = hotbar_index;
    update.inventory_slot_index = inventory_slot_index;
    if (inventory_slot_index >= 0 && inventory_slot_index < inventory.slotCount()) {
        const auto& stack = inventory.slot(inventory_slot_index);
        update.item_id = stack.item_id_;
        update.count = stack.count_;
    }
    upsertUpdate(updates, update);
}

} // namespace

HotbarSystem::HotbarSystem(entt::registry& registry, entt::dispatcher& dispatcher)
    : registry_(registry), dispatcher_(dispatcher) {
    subscribe();
}

HotbarSystem::~HotbarSystem() {
    unsubscribe();
}

void HotbarSystem::subscribe() {
    dispatcher_.sink<game::defs::HotbarBindCommand>().connect<&HotbarSystem::onBind>(this);
    dispatcher_.sink<game::defs::HotbarUnbindCommand>().connect<&HotbarSystem::onUnbind>(this);
    dispatcher_.sink<game::defs::HotbarSyncCommand>().connect<&HotbarSystem::onSync>(this);
    dispatcher_.sink<game::defs::InventoryChanged>().connect<&HotbarSystem::onInventoryChanged>(this);
}

void HotbarSystem::unsubscribe() {
    dispatcher_.sink<game::defs::HotbarBindCommand>().disconnect<&HotbarSystem::onBind>(this);
    dispatcher_.sink<game::defs::HotbarUnbindCommand>().disconnect<&HotbarSystem::onUnbind>(this);
    dispatcher_.sink<game::defs::HotbarSyncCommand>().disconnect<&HotbarSystem::onSync>(this);
    dispatcher_.sink<game::defs::InventoryChanged>().disconnect<&HotbarSystem::onInventoryChanged>(this);
}

bool HotbarSystem::validateTarget(entt::entity target) const {
    return registry_.valid(target) &&
           registry_.all_of<game::component::HotbarComponent, game::component::InventoryComponent>(target);
}

void HotbarSystem::emitChanged(entt::entity target, const std::vector<game::defs::HotbarSlotUpdate>& updates, bool full_sync) {
    game::defs::HotbarChanged evt{};
    evt.target = target;
    evt.slots = updates;
    evt.full_sync = full_sync;
    if (registry_.all_of<game::component::HotbarComponent>(target)) {
        evt.active_slot = registry_.get<game::component::HotbarComponent>(target).active_slot_index_;
    }
    dispatcher_.trigger(evt);
}

std::vector<game::defs::HotbarSlotUpdate> HotbarSystem::collectAll(entt::entity target) const {
    std::vector<game::defs::HotbarSlotUpdate> result;
    if (!validateTarget(target)) return result;

    const auto& hotbar = registry_.get<game::component::HotbarComponent>(target);
    const auto& inventory = registry_.get<game::component::InventoryComponent>(target);

    result.reserve(game::component::HotbarComponent::SLOT_COUNT);
    for (int i = 0; i < game::component::HotbarComponent::SLOT_COUNT; ++i) {
        const auto& hb_slot = hotbar.slot(i);
        game::defs::HotbarSlotUpdate update{};
        update.hotbar_index = i;
        update.inventory_slot_index = hb_slot.inventory_slot_index_;
        if (hb_slot.inventory_slot_index_ >= 0 && hb_slot.inventory_slot_index_ < inventory.slotCount()) {
            const auto& stack = inventory.slot(hb_slot.inventory_slot_index_);
            update.item_id = stack.item_id_;
            update.count = stack.count_;
        }
        result.push_back(update);
    }
    return result;
}

void HotbarSystem::onBind(const game::defs::HotbarBindCommand& evt) {
    if (!validateTarget(evt.target)) return;
    if (evt.hotbar_index < 0 || evt.hotbar_index >= game::component::HotbarComponent::SLOT_COUNT) return;

    auto& hotbar = registry_.get<game::component::HotbarComponent>(evt.target);
    const auto& inventory = registry_.get<game::component::InventoryComponent>(evt.target);

    if (evt.inventory_slot < 0 || evt.inventory_slot >= inventory.slotCount()) return;

    std::vector<game::defs::HotbarSlotUpdate> updates;
    updates.reserve(2);

    // 保证：一个物品栏槽位最多只能绑定到一个快捷栏槽位。
    // 若该 inventory_slot 已经在其他快捷栏位置上，先解绑旧位置（相当于“移动快捷键”而不是新增一个）。
    for (int i = 0; i < game::component::HotbarComponent::SLOT_COUNT; ++i) {
        if (i == evt.hotbar_index) continue;
        auto& other = hotbar.slot(i);
        if (other.inventory_slot_index_ != evt.inventory_slot) continue;

        other.inventory_slot_index_ = -1;

        game::defs::HotbarSlotUpdate update{};
        update.hotbar_index = i;
        update.inventory_slot_index = -1;
        updates.push_back(update);
    }

    auto& slot = hotbar.slot(evt.hotbar_index);
    if (slot.inventory_slot_index_ != evt.inventory_slot) {
        slot.inventory_slot_index_ = evt.inventory_slot;

        game::defs::HotbarSlotUpdate update{};
        update.hotbar_index = evt.hotbar_index;
        update.inventory_slot_index = evt.inventory_slot;

        const auto& stack = inventory.slot(evt.inventory_slot);
        update.item_id = stack.item_id_;
        update.count = stack.count_;

        updates.push_back(update);
    }

    if (!updates.empty()) {
        emitChanged(evt.target, updates, false);
    }
}

void HotbarSystem::onUnbind(const game::defs::HotbarUnbindCommand& evt) {
    if (!validateTarget(evt.target)) return;
    if (evt.hotbar_index < 0 || evt.hotbar_index >= game::component::HotbarComponent::SLOT_COUNT) return;

    auto& hotbar = registry_.get<game::component::HotbarComponent>(evt.target);
    auto& slot = hotbar.slot(evt.hotbar_index);
    if (slot.inventory_slot_index_ == -1) return;

    slot.inventory_slot_index_ = -1;

    game::defs::HotbarSlotUpdate update{};
    update.hotbar_index = evt.hotbar_index;
    update.inventory_slot_index = -1;
    emitChanged(evt.target, {update}, false);
}

void HotbarSystem::onSync(const game::defs::HotbarSyncCommand& evt) {
    if (!validateTarget(evt.target)) return;

    auto& hotbar = registry_.get<game::component::HotbarComponent>(evt.target);
    const auto& inventory = registry_.get<game::component::InventoryComponent>(evt.target);

    // 归一化无效映射：避免 hotbar 长期持有“悬空引用”（越界/非法负数）。
    for (int i = 0; i < game::component::HotbarComponent::SLOT_COUNT; ++i) {
        const int inv_index = hotbar.slot(i).inventory_slot_index_;
        if (inv_index == -1) continue;
        const bool valid = inv_index >= 0 && inv_index < inventory.slotCount();
        if (!valid) {
            hotbar.slot(i).inventory_slot_index_ = -1;
        }
    }

    emitChanged(evt.target, collectAll(evt.target), evt.full_sync);
}

void HotbarSystem::onInventoryChanged(const game::defs::InventoryChanged& evt) {
    if (!validateTarget(evt.target)) return;
    if (evt.slots.empty()) return;

    auto& hotbar = registry_.get<game::component::HotbarComponent>(evt.target);
    const auto& inventory = registry_.get<game::component::InventoryComponent>(evt.target);

    std::vector<game::defs::HotbarSlotUpdate> updates;
    updates.reserve(game::component::HotbarComponent::SLOT_COUNT);

    for (int hb_index = 0; hb_index < game::component::HotbarComponent::SLOT_COUNT; ++hb_index) {
        const int inv_index = hotbar.slot(hb_index).inventory_slot_index_;
        if (inv_index < 0) continue;
        if (!inventorySlotAffected(evt, inv_index)) continue;

        const bool valid_index = inv_index >= 0 && inv_index < inventory.slotCount();
        const bool slot_empty = valid_index ? inventory.slot(inv_index).empty() : true;
        if (!valid_index || slot_empty) {
            hotbar.slot(hb_index).inventory_slot_index_ = -1;
            pushSlotUpdate(updates, inventory, hb_index, -1);
            continue;
        }

        pushSlotUpdate(updates, inventory, hb_index, inv_index);
    }

    if (evt.from_add) {
        std::vector<entt::id_type> candidates;
        candidates.reserve(evt.slots.size());
        for (const auto& slot : evt.slots) {
            if (slot.item_id == entt::null || slot.count <= 0) continue;
            if (std::find(candidates.begin(), candidates.end(), slot.item_id) == candidates.end()) {
                candidates.push_back(slot.item_id);
            }
        }

        for (const entt::id_type item_id : candidates) {
            if (isItemOnHotbar(hotbar, inventory, item_id)) {
                continue;
            }

            const auto hotbar_index_to_fill = findHotbarSlotToFill(hotbar, inventory);
            if (!hotbar_index_to_fill) {
                break;
            }

            const int inventory_slot_to_bind = selectInventorySlotForItem(evt.slots, inventory, item_id);
            if (inventory_slot_to_bind < 0) {
                continue;
            }

            for (int i = 0; i < game::component::HotbarComponent::SLOT_COUNT; ++i) {
                if (i == *hotbar_index_to_fill) continue;
                if (hotbar.slot(i).inventory_slot_index_ != inventory_slot_to_bind) continue;
                hotbar.slot(i).inventory_slot_index_ = -1;
                pushSlotUpdate(updates, inventory, i, -1);
            }

            if (hotbar.slot(*hotbar_index_to_fill).inventory_slot_index_ != inventory_slot_to_bind) {
                hotbar.slot(*hotbar_index_to_fill).inventory_slot_index_ = inventory_slot_to_bind;
                pushSlotUpdate(updates, inventory, *hotbar_index_to_fill, inventory_slot_to_bind);
            }
        }
    }

    if (!updates.empty()) {
        emitChanged(evt.target, updates, false);
    }
}

} // namespace game::system
