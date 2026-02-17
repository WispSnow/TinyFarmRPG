#include "inventory_system.h"
#include "game/component/hotbar_component.h"
#include "game/component/inventory_component.h"
#include "game/data/item_catalog.h"
#include "game/domain/inventory_domain_service.h"

#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>

#include <algorithm>

namespace game::system {

namespace {

bool hotbarReferencesInventorySlot(const game::component::HotbarComponent& hotbar, int inventory_slot) {
    for (int i = 0; i < game::component::HotbarComponent::SLOT_COUNT; ++i) {
        if (hotbar.slot(i).inventory_slot_index_ == inventory_slot) {
            return true;
        }
    }
    return false;
}

void swapHotbarInventorySlotMappings(game::component::HotbarComponent& hotbar, int a, int b) {
    if (a == b) return;
    for (int i = 0; i < game::component::HotbarComponent::SLOT_COUNT; ++i) {
        auto& slot = hotbar.slot(i);
        if (slot.inventory_slot_index_ == a) {
            slot.inventory_slot_index_ = b;
        } else if (slot.inventory_slot_index_ == b) {
            slot.inventory_slot_index_ = a;
        }
    }
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
    dispatcher_.sink<game::defs::InventorySetActivePageCommand>().connect<&InventorySystem::onSetActivePage>(this);
}

void InventorySystem::unsubscribe() {
    dispatcher_.sink<game::defs::AddItemCommand>().disconnect<&InventorySystem::onAddItem>(this);
    dispatcher_.sink<game::defs::RemoveItemCommand>().disconnect<&InventorySystem::onRemoveItem>(this);
    dispatcher_.sink<game::defs::InventorySyncCommand>().disconnect<&InventorySystem::onSync>(this);
    dispatcher_.sink<game::defs::InventoryMoveCommand>().disconnect<&InventorySystem::onMoveItem>(this);
    dispatcher_.sink<game::defs::InventorySetActivePageCommand>().disconnect<&InventorySystem::onSetActivePage>(this);
}

bool InventorySystem::ensureInventory(entt::entity target) {
    if (!registry_.valid(target)) return false;
    if (!registry_.all_of<game::component::InventoryComponent>(target)) {
        registry_.emplace<game::component::InventoryComponent>(target);
    }
    return true;
}

void InventorySystem::emitChanged(entt::entity target, const std::vector<game::defs::InventorySlotUpdate>& diff, bool full_sync, int active_page) {
    game::defs::InventoryChanged evt{};
    evt.target = target;
    evt.slots = diff;
    evt.full_sync = full_sync;
    evt.active_page = active_page;
    evt.from_add = false;
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

    std::vector<game::defs::InventorySlotUpdate> all;
    all.reserve(static_cast<std::size_t>(inv.slotCount()));
    for (int i = 0; i < inv.slotCount(); ++i) {
        const auto& stack = inv.slot(i);
        all.push_back({i, stack.item_id_, stack.count_});
    }
    emitChanged(evt.target, all, true, inv.active_page_);
}

void InventorySystem::onSetActivePage(const game::defs::InventorySetActivePageCommand& evt) {
    if (evt.target == entt::null) return;
    if (!ensureInventory(evt.target)) return;

    auto& inv = registry_.get<game::component::InventoryComponent>(evt.target);
    inv.active_page_ = std::clamp(evt.active_page, 0, game::component::InventoryComponent::PAGE_COUNT - 1);
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

    auto* hotbar = registry_.try_get<game::component::HotbarComponent>(evt.target);

    const auto* item = catalog_.findItem(from.item_id_);
    const int stack_limit = item ? item->stack_limit_ : 999;

    std::vector<game::defs::InventorySlotUpdate> diff;
    diff.reserve(2);

    if (evt.allow_merge && !to.empty() && to.item_id_ == from.item_id_ && to.count_ < stack_limit) {
        const int space = stack_limit - to.count_;
        const int moved = std::min(space, from.count_);
        to.count_ += moved;
        from.count_ -= moved;
        if (from.count_ <= 0) {
            from.clear();
        }
        if (from.empty() && hotbar && hotbarReferencesInventorySlot(*hotbar, evt.from_slot)) {
            // 快捷栏“跟随物品”而不是“跟随槽位”：
            // - 一般情况下，移动/交换物品时会同步交换 hotbar 的 inventory_slot_index_ 映射，让 hotkey 继续指向原物品。
            // - 合并堆叠（merge）会让 source 槽位清空：若 target 槽位也被 hotkey 引用，则必须清空 source 的 hotkey，
            //   但该“清空”应由 HotbarSystem 在 InventoryChanged 中统一收敛并发出 HotbarChanged，
            //   不能在这里直接改映射，否则会跳过 HotbarChanged 事件链。
            const bool target_has_hotkey = hotbarReferencesInventorySlot(*hotbar, evt.to_slot);
            if (!target_has_hotkey) {
                swapHotbarInventorySlotMappings(*hotbar, evt.from_slot, evt.to_slot);
            }
        }
        diff.push_back({evt.from_slot, from.item_id_, from.count_});
        diff.push_back({evt.to_slot, to.item_id_, to.count_});
    } else if (to.empty()) {
        to = from;
        from.clear();
        if (hotbar && hotbarReferencesInventorySlot(*hotbar, evt.from_slot)) {
            swapHotbarInventorySlotMappings(*hotbar, evt.from_slot, evt.to_slot);
        }
        diff.push_back({evt.from_slot, from.item_id_, from.count_});
        diff.push_back({evt.to_slot, to.item_id_, to.count_});
    } else {
        std::swap(from, to);
        if (hotbar) {
            swapHotbarInventorySlotMappings(*hotbar, evt.from_slot, evt.to_slot);
        }
        diff.push_back({evt.from_slot, from.item_id_, from.count_});
        diff.push_back({evt.to_slot, to.item_id_, to.count_});
    }

    if (!diff.empty()) {
        emitChanged(evt.target, diff, false, inv.active_page_);
    }
}

} // namespace game::system
