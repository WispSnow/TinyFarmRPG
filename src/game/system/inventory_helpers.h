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
