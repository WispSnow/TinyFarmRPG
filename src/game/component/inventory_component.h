#pragma once

#include "game/defs/constants.h"
#include "game/defs/crop_defs.h"
#include <cassert>
#include <entt/entity/entity.hpp>
#include <vector>

namespace game::component {

/// @brief 单个物品堆
struct ItemStack {
    entt::id_type item_id_{entt::null};
    int count_{0};

    bool empty() const { return item_id_ == entt::null || count_ <= 0; }
    void clear() {
        item_id_ = entt::null;
        count_ = 0;
    }
};

/// @brief 玩家背包组件（两页，每页 5x4）
struct InventoryComponent {
    static constexpr int COLUMNS = 5;
    static constexpr int ROWS = 4;
    static constexpr int SLOTS_PER_PAGE = COLUMNS * ROWS;
    static constexpr int PAGE_COUNT = 2;
    static constexpr int TOTAL_SLOTS = SLOTS_PER_PAGE * PAGE_COUNT;

    std::vector<ItemStack> slots_{static_cast<std::size_t>(TOTAL_SLOTS)};   // 使用vector，未来有拓展可能
    int active_page_{0};  ///< 0 or 1

    [[nodiscard]] int slotCount() const { return static_cast<int>(slots_.size()); }
    [[nodiscard]] ItemStack& slot(int index) { assert(index >= 0 && index < slotCount()); return slots_[static_cast<std::size_t>(index)]; }
    [[nodiscard]] const ItemStack& slot(int index) const { assert(index >= 0 && index < slotCount()); return slots_[static_cast<std::size_t>(index)]; }

    void clearAll() {
        for (auto& s : slots_) {
            s.clear();
        }
    }
};

} // namespace game::component
