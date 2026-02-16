#include "inventory_system.h"
#include "inventory_helpers.h"
#include "game/component/hotbar_component.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <optional>

namespace game::system {

using detail::tryMerge;
using detail::tryFillEmpty;

namespace {

std::optional<int> findHotbarSlotToFill(const game::component::HotbarComponent& hotbar,
                                        const game::component::InventoryComponent& inv) {
    for (int i = 0; i < game::component::HotbarComponent::SLOT_COUNT; ++i) {
        if (hotbar.slot(i).empty()) {
            return i;
        }
    }

    for (int i = 0; i < game::component::HotbarComponent::SLOT_COUNT; ++i) {
        const int inv_index = hotbar.slot(i).inventory_slot_index_;
        if (inv_index < 0 || inv_index >= inv.slotCount()) {
            return i;
        }
        if (inv.slot(inv_index).empty()) {
            return i;
        }
    }

    return std::nullopt;
}

bool isItemOnHotbar(const game::component::HotbarComponent& hotbar,
                    const game::component::InventoryComponent& inv,
                    entt::id_type item_id) {
    for (int i = 0; i < game::component::HotbarComponent::SLOT_COUNT; ++i) {
        const int inv_index = hotbar.slot(i).inventory_slot_index_;
        if (inv_index < 0 || inv_index >= inv.slotCount()) continue;
        const auto& stack = inv.slot(inv_index);
        if (!stack.empty() && stack.item_id_ == item_id) {
            return true;
        }
    }
    return false;
}

bool hotbarReferencesInventorySlot(const game::component::HotbarComponent& hotbar, int inventory_slot) {
    for (int i = 0; i < game::component::HotbarComponent::SLOT_COUNT; ++i) {
        if (hotbar.slot(i).inventory_slot_index_ == inventory_slot) {
            return true;
        }
    }
    return false;
}

std::vector<int> collectHotbarSlotIndicesReferencingInventorySlot(const game::component::HotbarComponent& hotbar,
                                                                  int inventory_slot) {
    std::vector<int> result;
    for (int i = 0; i < game::component::HotbarComponent::SLOT_COUNT; ++i) {
        if (hotbar.slot(i).inventory_slot_index_ == inventory_slot) {
            result.push_back(i);
        }
    }
    return result;
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

int selectInventorySlotForItem(const std::vector<game::defs::InventorySlotUpdate>& diff,
                               const game::component::InventoryComponent& inv,
                               entt::id_type item_id) {
    int best = -1;
    for (const auto& update : diff) {
        if (update.item_id != item_id || update.count <= 0) continue;
        best = best < 0 ? update.slot_index : std::min(best, update.slot_index);
    }

    if (best >= 0) return best;

    for (int i = 0; i < inv.slotCount(); ++i) {
        const auto& stack = inv.slot(i);
        if (!stack.empty() && stack.item_id_ == item_id) {
            return i;
        }
    }
    return -1;
}

} // namespace

InventorySystem::InventorySystem(entt::registry& registry, entt::dispatcher& dispatcher, game::data::ItemCatalog& catalog)
    : registry_(registry), dispatcher_(dispatcher), catalog_(catalog) {
    subscribe();
}

InventorySystem::~InventorySystem() {
    unsubscribe();
}

void InventorySystem::subscribe() {
    dispatcher_.sink<game::defs::AddItemRequest>().connect<&InventorySystem::onAddItem>(this);
    dispatcher_.sink<game::defs::RemoveItemRequest>().connect<&InventorySystem::onRemoveItem>(this);
    dispatcher_.sink<game::defs::InventorySyncRequest>().connect<&InventorySystem::onSync>(this);
    dispatcher_.sink<game::defs::InventoryMoveRequest>().connect<&InventorySystem::onMoveItem>(this);
    dispatcher_.sink<game::defs::InventorySetActivePageRequest>().connect<&InventorySystem::onSetActivePage>(this);
}

void InventorySystem::unsubscribe() {
    dispatcher_.sink<game::defs::AddItemRequest>().disconnect<&InventorySystem::onAddItem>(this);
    dispatcher_.sink<game::defs::RemoveItemRequest>().disconnect<&InventorySystem::onRemoveItem>(this);
    dispatcher_.sink<game::defs::InventorySyncRequest>().disconnect<&InventorySystem::onSync>(this);
    dispatcher_.sink<game::defs::InventoryMoveRequest>().disconnect<&InventorySystem::onMoveItem>(this);
    dispatcher_.sink<game::defs::InventorySetActivePageRequest>().disconnect<&InventorySystem::onSetActivePage>(this);
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
    dispatcher_.trigger(evt);
}

void InventorySystem::onAddItem(const game::defs::AddItemRequest& evt) {
    if (evt.target == entt::null || evt.item_id == entt::null || evt.count <= 0) return;
    if (!ensureInventory(evt.target)) return;

    auto& inv = registry_.get<game::component::InventoryComponent>(evt.target);
    const auto* item = catalog_.findItem(evt.item_id);
    const int stack_limit = item ? item->stack_limit_ : 999;
    int remaining = evt.count;
    std::vector<game::defs::InventorySlotUpdate> diff;
    diff.reserve(4);

    // 优先尝试指定槽位（用于“使用物品产物尽量回填原槽位”等体验优化）
    if (evt.preferred_slot_index >= 0 && evt.preferred_slot_index < inv.slotCount() && remaining > 0) {
        auto& stack = inv.slot(evt.preferred_slot_index);
        const auto before_id = stack.item_id_;
        const int before_count = stack.count_;
        remaining = tryMerge(stack, evt.item_id, remaining, stack_limit);
        remaining = tryFillEmpty(stack, evt.item_id, remaining, stack_limit);
        if (stack.item_id_ != before_id || stack.count_ != before_count) {
            diff.push_back({evt.preferred_slot_index, stack.item_id_, stack.count_});
        }
    }

    // 先合并同类
    for (int i = 0; i < inv.slotCount() && remaining > 0; ++i) {
        auto& stack = inv.slot(i);
        int before = stack.count_;
        remaining = tryMerge(stack, evt.item_id, remaining, stack_limit);
        if (stack.count_ != before) {
            diff.push_back({i, stack.item_id_, stack.count_});
        }
    }

    // 再填充空位
    for (int i = 0; i < inv.slotCount() && remaining > 0; ++i) {
        auto& stack = inv.slot(i);
        int before = stack.count_;
        remaining = tryFillEmpty(stack, evt.item_id, remaining, stack_limit);
        if (stack.count_ != before) {
            diff.push_back({i, stack.item_id_, stack.count_});
        }
    }

    if (!diff.empty()) {
        emitChanged(evt.target, diff, false, inv.active_page_);
    }

    // 自动将新获得的物品绑定到快捷栏空位（快捷栏只是物品栏槽位的引用）
    if (!diff.empty() && registry_.all_of<game::component::HotbarComponent>(evt.target)) {
        const auto& hotbar = registry_.get<game::component::HotbarComponent>(evt.target);
        if (!isItemOnHotbar(hotbar, inv, evt.item_id)) {
            const auto slot_to_fill = findHotbarSlotToFill(hotbar, inv);
            if (slot_to_fill) {
                const int inv_slot = selectInventorySlotForItem(diff, inv, evt.item_id);
                if (inv_slot >= 0) {
                    dispatcher_.trigger(game::defs::HotbarBindRequest{evt.target, *slot_to_fill, inv_slot});
                }
            }
        }
    }

    if (remaining > 0) {
        game::defs::InventoryFullEvent full_evt{};
        full_evt.target = evt.target;
        full_evt.item_id = evt.item_id;
        full_evt.rejected = remaining;
        dispatcher_.trigger(full_evt);
    }
}

void InventorySystem::onRemoveItem(const game::defs::RemoveItemRequest& evt) {
    if (evt.target == entt::null || evt.item_id == entt::null || evt.count <= 0) return;
    if (!registry_.valid(evt.target) || !registry_.all_of<game::component::InventoryComponent>(evt.target)) return;

    auto& inv = registry_.get<game::component::InventoryComponent>(evt.target);
    int remaining = evt.count;
    std::vector<game::defs::InventorySlotUpdate> diff;
    diff.reserve(4);

    auto removeFromSlot = [&](int slot_index) {
        if (slot_index < 0 || slot_index >= inv.slotCount() || remaining <= 0) return;
        auto& stack = inv.slot(slot_index);
        if (stack.empty() || stack.item_id_ != evt.item_id) return;
        const int take = std::min(stack.count_, remaining);
        stack.count_ -= take;
        remaining -= take;
        if (stack.count_ <= 0) {
            stack.clear();
        }
        diff.push_back({slot_index, stack.item_id_, stack.count_});
    };

    if (evt.slot_index >= 0) {
        removeFromSlot(evt.slot_index);
    } else {
        for (int i = 0; i < inv.slotCount() && remaining > 0; ++i) {
            removeFromSlot(i);
        }
    }

    if (!diff.empty()) {
        emitChanged(evt.target, diff, false, inv.active_page_);
    }
}

void InventorySystem::onSync(const game::defs::InventorySyncRequest& evt) {
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

void InventorySystem::onSetActivePage(const game::defs::InventorySetActivePageRequest& evt) {
    if (evt.target == entt::null) return;
    if (!ensureInventory(evt.target)) return;

    auto& inv = registry_.get<game::component::InventoryComponent>(evt.target);
    inv.active_page_ = std::clamp(evt.active_page, 0, game::component::InventoryComponent::PAGE_COUNT - 1);
}

void InventorySystem::onMoveItem(const game::defs::InventoryMoveRequest& evt) {
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
            //   否则 swap 会把 target 的 hotkey 交换到空槽位，导致一个 hotkey 被“清空”（swap 的副作用）。
            const bool target_has_hotkey = hotbarReferencesInventorySlot(*hotbar, evt.to_slot);
            if (target_has_hotkey) {
                // Rule A: 保留 target(to) 的 hotkey，清空 source(from) 的 hotkey。
                const auto indices = collectHotbarSlotIndicesReferencingInventorySlot(*hotbar, evt.from_slot);
                for (const int hb_index : indices) {
                    dispatcher_.trigger(game::defs::HotbarUnbindRequest{evt.target, hb_index});
                    // fallback：若 HotbarSystem 不存在，至少保证组件状态正确（GameScene 可通过 InventoryChanged 兜底刷新 UI）。
                    hotbar->slot(hb_index).inventory_slot_index_ = -1;
                }
            } else {
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
