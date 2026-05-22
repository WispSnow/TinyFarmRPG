#pragma once

#include <entt/core/fwd.hpp>
#include <entt/entity/entity.hpp>

#include <optional>
#include <string>

namespace game::defs {

struct AddItemCommand {
    entt::entity target{entt::null};
    entt::id_type item_id{entt::null};
    int count{0};
    int preferred_slot_index{-1};   ///< @brief 如果>=0，优先尝试放入指定槽位
};

struct RemoveItemCommand {
    entt::entity target{entt::null};
    entt::id_type item_id{entt::null};
    int count{0};
    int slot_index{-1};   ///< @brief 如果>=0，仅从指定槽位扣除
};

struct UseItemCommand {
    entt::entity target{entt::null};
    int inventory_slot_index{-1};
    int count{1};
    bool show_prompt{false};   ///< @brief 是否需要弹出提示（物品栏=true，快捷栏=false）
    std::optional<std::string> actor_target_id{}; ///< @brief 菜单中对队友使用战斗道具时的目标 actor id。
};

struct InventorySyncCommand {
    entt::entity target{entt::null};
};

struct InventoryMoveCommand {
    entt::entity target{entt::null};
    int from_slot{-1};
    int to_slot{-1};
    bool allow_merge{true};
};

struct InventorySortCommand {
    entt::entity target{entt::null};
};

struct HotbarBindCommand {
    entt::entity target{entt::null};
    int hotbar_index{-1};
    int inventory_slot{-1};
};

struct HotbarUnbindCommand {
    entt::entity target{entt::null};
    int hotbar_index{-1};
};

struct HotbarActivateCommand {
    entt::entity target{entt::null};
    int hotbar_index{-1};
};

struct HotbarSyncCommand {
    entt::entity target{entt::null};
    bool full_sync{true};
};

} // namespace game::defs
