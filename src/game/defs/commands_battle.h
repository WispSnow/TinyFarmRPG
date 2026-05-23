#pragma once

#include "game/battle/battle_types.h"

#include <entt/core/fwd.hpp>
#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>

#include <optional>
#include <string>
#include <vector>

namespace game::defs {

struct EnemyEncounterBattleContext {
    entt::entity source_entity{entt::null};
    entt::id_type map_id{entt::null};
    int encounter_id{0};
    std::string troop_id{};
    bool respawn_on_map_reload{true};
    glm::vec2 home_position{0.0f, 0.0f};
};

/// @brief 请求从探索流程进入一场回合制战斗。
///
/// 命令可以直接携带已构造好的 BattleUnit，也可以只携带 actor/troop id，
/// 由 GameScene 通过 BattleUnit 工厂和 RPG 目录构建战斗单位。
struct EnterBattleCommand {
    std::vector<std::string> actor_ids{};                         ///< 需要加入玩家方的 actor id；为空时由工厂选择默认玩家队伍。
    std::string troop_id{};                                       ///< 要加载的敌方 troop id；为空时由工厂选择默认敌群。
    std::string battle_background_id{};                           ///< 可选战斗背景逻辑 id；为空时由地图/troop/default 解析。
    std::vector<game::battle::BattleUnit> player_units{};         ///< 已预构建的玩家方战斗单位；非空时可绕过 actor/class 装配。
    std::vector<game::battle::BattleUnit> enemy_units{};          ///< 已预构建的敌方战斗单位；非空时可绕过 troop/enemy 装配。
    std::optional<EnemyEncounterBattleContext> encounter_context{};///< 地图触发遭遇的结算上下文；手动战斗入口为空。
};

/// @brief 表现层提交战斗行动的命令负载。
struct SubmitBattleActionCommand {
    game::battle::BattleAction action{};    ///< 要交给 BattleSession 校验并执行的行动意图。
};

} // namespace game::defs
