#pragma once

#include <entt/core/fwd.hpp>
#include <entt/entity/entity.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace game::defs {

struct InventorySlotUpdate {
    int slot_index{0};
    entt::id_type item_id{entt::null};
    int count{0};
};

enum class InventoryMoveKind : std::uint8_t {
    None = 0,
    MoveToEmpty,
    Swap,
    Merge
};

struct InventoryChanged {
    entt::entity target{entt::null};
    std::vector<InventorySlotUpdate> slots{};
    bool full_sync{false};
    bool from_add{false};   ///< @brief 是否由“加物品”语义触发，用于保持 Hotbar 自动绑定策略稳定
    InventoryMoveKind move_kind{InventoryMoveKind::None}; ///< @brief move 语义（仅 onMoveItem 路径设置）
    int move_from_slot{-1};                               ///< @brief move 源槽位
    int move_to_slot{-1};                                 ///< @brief move 目标槽位
};

struct InventoryFullEvent {
    entt::entity target{entt::null};
    entt::id_type item_id{entt::null};
    int rejected{0};
};

struct ItemUsedEvent {
    entt::entity target{entt::null};
    entt::id_type item_id{entt::null};
    int inventory_slot_index{-1};
    int count{0};
    std::optional<std::string> actor_target_id{};
};

struct HotbarSlotChanged {
    entt::entity target{entt::null};
    int slot_index{0};  ///< 高亮的槽位索引；-1 表示当前不高亮任何槽位
};

struct HotbarSlotUpdate {
    int hotbar_index{0};
    int inventory_slot_index{-1};
    entt::id_type item_id{entt::null};
    int count{0};
};

struct HotbarChanged {
    entt::entity target{entt::null};
    std::vector<HotbarSlotUpdate> slots{};
    bool full_sync{false};
    int active_slot{0};
};

} // namespace game::defs
