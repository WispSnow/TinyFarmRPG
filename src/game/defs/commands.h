#pragma once

#include <entt/core/fwd.hpp>
#include <entt/entity/entity.hpp>

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

struct InventorySetActivePageCommand {
    entt::entity target{entt::null};
    int active_page{0};
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

// 交互意图命令（InteractionSystem 的“扩展点”）
// - InteractionSystem 只负责：选目标 + 发布命令（不写具体交互逻辑）
// - 具体交互由订阅者系统各自处理（Dialogue/Chest/Rest...）
// - 想加新交互对象时：优先新增订阅者，而不是改 InteractionSystem
struct InteractCommand {
    entt::entity player{entt::null};
    entt::entity target{entt::null};
};

} // namespace game::defs
