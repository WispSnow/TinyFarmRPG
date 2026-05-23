#pragma once

#include "engine/utils/events.h"
#include "engine/utils/math.h"
#include "engine/vfx/vfx_types.h"
#include "game/battle/battle_types.h"

#include <glm/vec2.hpp>

#include <optional>
#include <string>
#include <variant>

namespace game::scene {

/// @brief 单帧同步推进的战斗流程状态。
enum class BattleFlowState {
    WaitingForInput,    ///< 等待玩家输入行动。
    ExecutingAction,    ///< 将 pending_action_ 提交给 BattleSession。
    AnimatingResult,    ///< 展示动作结果；当前以短计时占位。
    CheckVictory,       ///< 根据 BattleActionResult::outcome_after 判断是否结束战斗。
    VictoryFlow,        ///< 展示 Victory 结算流程，等待玩家确认后退出。
    NextTurn,           ///< 刷新到下一个行动者并路由到玩家输入或敌方自动行动。
    BattleEnd           ///< 发送结算事件并请求弹出场景。
};

/// @brief 当前菜单上下文，用于区分主菜单、技能列表、道具列表和目标选择等不同输入语义。
enum class BattleMenuState {
    None,
    PartyCommand,
    ActorCommand,
    SkillList,
    ItemList,
    TargetSelect
};

/// @brief 记录玩家在菜单中逐步拼装中的行动草稿。
struct BattleActionDraft {
    game::battle::BattleActionType pending_type{game::battle::BattleActionType::EndTurn};
    std::optional<std::string> selected_skill_id{};
    std::optional<std::string> selected_item_id{};
    std::optional<game::battle::BattleUnitId> selected_target_id{};
    bool requires_target_selection{false};
};

/// @brief 进入战斗前的相机状态，BattleScene 退出时恢复探索态相机。
struct BattleCameraStateSnapshot {
    glm::vec2 position{0.0F};
    float zoom{1.0F};
    float rotation{0.0F};
    float min_zoom{0.1F};
    float max_zoom{10.0F};
    std::optional<engine::utils::Rect> limit_bounds{};
};

/// @brief 动作表现层时间轴中延迟触发的副作用事件。
struct BattleScheduledPresentationEvent {
    std::variant<engine::vfx::PlayVfxCommand, engine::utils::PlaySoundEvent, game::battle::BattleActionResult> payload{};
    float remaining_seconds{0.0F};
};

} // namespace game::scene
