#include "hotbar_system.h"
#include <spdlog/spdlog.h>
#include <algorithm>

namespace game::system {

HotbarSystem::HotbarSystem(entt::registry& registry, entt::dispatcher& dispatcher)
    : registry_(registry), dispatcher_(dispatcher) {
    subscribe();
}

HotbarSystem::~HotbarSystem() {
    unsubscribe();
}

void HotbarSystem::subscribe() {
    dispatcher_.sink<game::defs::HotbarBindRequest>().connect<&HotbarSystem::onBind>(this);
    dispatcher_.sink<game::defs::HotbarUnbindRequest>().connect<&HotbarSystem::onUnbind>(this);
    dispatcher_.sink<game::defs::HotbarSyncRequest>().connect<&HotbarSystem::onSync>(this);
    dispatcher_.sink<game::defs::InventoryChanged>().connect<&HotbarSystem::onInventoryChanged>(this);
}

void HotbarSystem::unsubscribe() {
    dispatcher_.sink<game::defs::HotbarBindRequest>().disconnect<&HotbarSystem::onBind>(this);
    dispatcher_.sink<game::defs::HotbarUnbindRequest>().disconnect<&HotbarSystem::onUnbind>(this);
    dispatcher_.sink<game::defs::HotbarSyncRequest>().disconnect<&HotbarSystem::onSync>(this);
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

void HotbarSystem::onBind(const game::defs::HotbarBindRequest& evt) {
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

void HotbarSystem::onUnbind(const game::defs::HotbarUnbindRequest& evt) {
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

void HotbarSystem::onSync(const game::defs::HotbarSyncRequest& evt) {
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

    const auto& hotbar = registry_.get<game::component::HotbarComponent>(evt.target);
    const auto& inventory = registry_.get<game::component::InventoryComponent>(evt.target);

    std::vector<game::defs::HotbarSlotUpdate> updates;
    updates.reserve(game::component::HotbarComponent::SLOT_COUNT);

    for (int hb_index = 0; hb_index < game::component::HotbarComponent::SLOT_COUNT; ++hb_index) {
        const int inv_index = hotbar.slot(hb_index).inventory_slot_index_;
        if (inv_index < 0) continue;

        const bool affected = std::any_of(evt.slots.begin(), evt.slots.end(), [inv_index](const auto& s) {
            return s.slot_index == inv_index;
        });
        if (!affected) continue;

        game::defs::HotbarSlotUpdate update{};
        update.hotbar_index = hb_index;
        update.inventory_slot_index = inv_index;
        if (inv_index < inventory.slotCount()) {
            const auto& stack = inventory.slot(inv_index);
            update.item_id = stack.item_id_;
            update.count = stack.count_;
        }
        updates.push_back(update);
    }

    if (!updates.empty()) {
        emitChanged(evt.target, updates, false);
    }
}

} // namespace game::system
