#pragma once

#include "game/component/inventory_component.h"
#include "game/data/item_catalog.h"
#include <algorithm>

namespace game::system::detail {

/// @brief 尝试将 count 个 item_id 合并到已有同类堆叠中，返回剩余未放入数量
inline int tryMerge(game::component::ItemStack& stack, entt::id_type item_id, int count, int stack_limit) {
    if (stack.empty() || stack.item_id_ != item_id) return count;
    const int space = stack_limit - stack.count_;
    if (space <= 0) return count;
    const int to_add = std::min(space, count);
    stack.count_ += to_add;
    return count - to_add;
}

/// @brief 尝试将 count 个 item_id 放入空槽位，返回剩余未放入数量
inline int tryFillEmpty(game::component::ItemStack& stack, entt::id_type item_id, int count, int stack_limit) {
    if (!stack.empty()) return count;
    const int to_add = std::min(stack_limit, count);
    stack.item_id_ = item_id;
    stack.count_ = to_add;
    return count - to_add;
}

/// @brief 在一份背包槽位副本上模拟 addItem 规则；全部放入返回 true
[[nodiscard]] inline bool simulateAdd(std::vector<game::component::ItemStack>& slots,
                                      int preferred_slot_index,
                                      entt::id_type item_id,
                                      int count,
                                      int stack_limit) {
    if (item_id == entt::null || count <= 0) {
        return true;
    }

    int remaining = count;

    if (preferred_slot_index >= 0 && preferred_slot_index < static_cast<int>(slots.size()) && remaining > 0) {
        remaining = tryMerge(slots[static_cast<std::size_t>(preferred_slot_index)], item_id, remaining, stack_limit);
        remaining = tryFillEmpty(slots[static_cast<std::size_t>(preferred_slot_index)], item_id, remaining, stack_limit);
    }

    for (auto& slot : slots) {
        if (remaining <= 0) {
            break;
        }
        remaining = tryMerge(slot, item_id, remaining, stack_limit);
    }

    for (auto& slot : slots) {
        if (remaining <= 0) {
            break;
        }
        remaining = tryFillEmpty(slot, item_id, remaining, stack_limit);
    }

    return remaining <= 0;
}

/// @brief 查询物品的堆叠上限，找不到则返回默认值 999
[[nodiscard]] inline int stackLimitOrDefault(const game::data::ItemCatalog* catalog, entt::id_type item_id) {
    constexpr int DEFAULT_STACK_LIMIT = 999;
    if (catalog != nullptr) {
        if (const auto* item = catalog->findItem(item_id)) {
            return std::max(1, item->stack_limit_);
        }
    }
    return DEFAULT_STACK_LIMIT;
}

} // namespace game::system::detail
