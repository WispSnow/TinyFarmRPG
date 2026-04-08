#pragma once

#include "game/battle/battle_types.h"

#include <entt/core/fwd.hpp>
#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <string>
#include <vector>

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

// 交互意图命令（InteractionSystem 的“扩展点”）
// - InteractionSystem 只负责：选目标 + 发布命令（不写具体交互逻辑）
// - 具体交互由订阅者系统各自处理（Dialogue/Chest/Rest...）
// - 想加新交互对象时：优先新增订阅者，而不是改 InteractionSystem
struct InteractCommand {
    entt::entity player{entt::null};
    entt::entity target{entt::null};
};

/// @brief 请求从探索流程进入一场回合制战斗。
///
/// 命令可以直接携带已构造好的 BattleUnit，也可以只携带 actor/troop id，
/// 由 GameScene 通过 BattleUnit 工厂和 RPG 目录构建战斗单位。
struct EnterBattleCommand {
    // TODO(FND-010): 若战斗命令继续膨胀，拆分到 commands_battle.h 以收敛全局头文件依赖。
    std::vector<std::string> actor_ids{};                         ///< 需要加入玩家方的 actor id；为空时由工厂选择默认玩家队伍。
    std::string troop_id{};                                       ///< 要加载的敌方 troop id；为空时由工厂选择默认敌群。
    std::vector<game::battle::BattleUnit> player_units{};         ///< 已预构建的玩家方战斗单位；非空时可绕过 actor/class 装配。
    std::vector<game::battle::BattleUnit> enemy_units{};          ///< 已预构建的敌方战斗单位；非空时可绕过 troop/enemy 装配。
};

/// @brief 表现层提交战斗行动的命令负载。
struct SubmitBattleActionCommand {
    game::battle::BattleAction action{};    ///< 要交给 BattleSession 校验并执行的行动意图。
};

struct SetAppearanceSlotCommand {
    entt::entity target{entt::null};
    std::string slot{};
    std::string variant{};
};

struct RefreshAppearanceCommand {
    entt::entity target{entt::null};
};

} // namespace game::defs
