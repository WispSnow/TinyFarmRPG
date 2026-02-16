#pragma once

#include <array>
#include <cassert>

namespace game::component {

/// @brief 快捷栏槽位信息
struct HotbarSlot {
    int inventory_slot_index_{-1};  ///< 对应的物品栏槽位索引，-1表示空槽位

    bool empty() const { return inventory_slot_index_ == -1; }
    void clear() { inventory_slot_index_ = -1; }
};

/// @brief 玩家快捷栏组件
struct HotbarComponent {
    static constexpr int SLOT_COUNT = 10;  ///< 快捷栏槽位数量

    std::array<HotbarSlot, SLOT_COUNT> slots_{};  ///< 快捷栏槽位数组
    int active_slot_index_{0};  ///< 当前激活的槽位索引 (0-9)

    [[nodiscard]] HotbarSlot& slot(int index) { assert(index >= 0 && index < SLOT_COUNT); return slots_[static_cast<std::size_t>(index)]; }
    [[nodiscard]] const HotbarSlot& slot(int index) const { assert(index >= 0 && index < SLOT_COUNT); return slots_[static_cast<std::size_t>(index)]; }

    /// @brief 设置快捷栏槽位对应的物品栏槽位
    void setSlotMapping(int hotbar_index, int inventory_index) {
        if (hotbar_index >= 0 && hotbar_index < SLOT_COUNT) {
            slots_[static_cast<std::size_t>(hotbar_index)].inventory_slot_index_ = inventory_index;
        }
    }

    /// @brief 获取当前激活槽位对应的物品栏槽位索引
    [[nodiscard]] int getActiveInventorySlot() const {
        return slots_[static_cast<std::size_t>(active_slot_index_)].inventory_slot_index_;
    }

    /// @brief 设置激活槽位
    void setActiveSlot(int index) {
        if (index >= 0 && index < SLOT_COUNT) {
            active_slot_index_ = index;
        }
    }

    void clearAll() {
        for (auto& slot : slots_) {
            slot.clear();
        }
    }
};

} // namespace game::component
