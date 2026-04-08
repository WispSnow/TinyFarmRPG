#pragma once

#include "engine/scene/scene.h"
#include "engine/ui/rmlui/rml_document_controller.h"
#include "game/battle/battle_session.h"

#include <RmlUi/Core/Types.h>

#include <optional>
#include <string_view>
#include <vector>

namespace game::scene {

/// @brief 回合制战斗表现层场景。
///
/// BattleScene 负责 UI、输入与状态机编排；领域规则通过 BattleSession 访问，
/// 不直接修改 TurnCore。该场景以 push/pop 方式叠加在探索场景之上，战斗结束后
/// 发出 BattleEndedEvent 并请求弹出自身。
class BattleScene final : public engine::scene::Scene {
    /// @brief 单帧同步推进的战斗流程状态。
    enum class FlowState {
        WaitingForInput,    ///< 等待玩家或未来 AI 提交行动。
        ExecutingAction,    ///< 将 pending_action_ 提交给 BattleSession。
        AnimatingResult,    ///< 展示动作结果；当前以短计时占位。
        CheckVictory,       ///< 根据 BattleActionResult::outcome_after 判断是否结束战斗。
        NextTurn,           ///< 刷新到下一个行动者并回到输入等待。
        BattleEnd           ///< 发送结算事件并请求弹出场景。
    };

    game::battle::BattleSession session_;                                               ///< 战斗应用层会话，负责动作提交与快照生成。
    FlowState state_{FlowState::WaitingForInput};                                      ///< 当前状态机节点。
    std::optional<game::battle::BattleAction> pending_action_{};                       ///< 等待执行的玩家/AI 行动。
    std::optional<game::battle::BattleActionResult> last_action_result_{};             ///< 最近一次动作提交结果，用于动画、文本与胜负检查。
    float animation_timer_{0.0f};                                                       ///< 结果动画占位计时器。
    bool end_requested_{false};                                                         ///< 防止重复发送战斗结束请求。
    bool context_pushed_{false};                                                        ///< 标记 RmlUi 上下文是否已经压栈。

    engine::ui::rmlui::RmlDocumentController document_controller_{};                    ///< 战斗 UI 文档控制器。

    Rml::String turn_text_{"Turn: -"};                                                  ///< 当前回合文本。
    Rml::String units_text_{"Units: -"};                                                ///< 单位状态文本。
    Rml::String result_text_{"Result: Choose action"};                                  ///< 最近行动结果文本。
    bool actions_enabled_{false};                                                       ///< UI 行动按钮是否可用。

public:
    /// @brief 构造战斗场景。
    /// @param name 场景名称。
    /// @param context 引擎上下文。
    /// @param units 初始战斗单位。
    /// @param session_options BattleSession 使用的技能/道具目录与库存依赖。
    BattleScene(std::string_view name,
                engine::core::Context& context,
                std::vector<game::battle::BattleUnit> units,
                game::battle::BattleSessionOptions session_options = {});
    ~BattleScene() override;

    /// @brief 初始化战斗场景和 UI。
    bool init() override;

    /// @brief 每帧推进战斗状态机。
    void update(float delta_time) override;

    /// @brief 清理战斗 UI 与场景资源。
    void clean() override;

private:
    /// @brief 初始化 RmlUi 战斗面板。
    [[nodiscard]] bool initUI();

    /// @brief 关闭 RmlUi 战斗面板。
    void shutdownUI();

    /// @brief 运行同步战斗流程状态机，直到进入需要等待的状态。
    void runStateMachine(float delta_time);

    /// @brief 根据 BattleSession 快照刷新 UI 文本和按钮状态。
    void refreshView();

    /// @brief 构造并排队普通攻击行动。
    void queueAttackAction();

    /// @brief 构造并排队默认技能行动。
    void queueSkillAction();

    /// @brief 构造并排队默认道具行动。
    void queueItemAction();

    /// @brief 构造并排队防御行动。
    void queueGuardAction();

    /// @brief 构造并排队逃跑行动。
    void queueEscapeAction();

    /// @brief 构造并排队跳过回合行动。
    void queueEndTurnAction();

    /// @brief 获取当前行动者并写出 actor id。
    /// @return 当前行动者存在时返回单位指针，否则返回 nullptr。
    [[nodiscard]] const game::battle::BattleUnit* prepareActionActor(game::battle::BattleUnitId& out_actor_id) const;

    /// @brief 为当前行动者选择默认敌方目标。
    /// @param actor_side 当前行动者阵营。
    /// @return 首个存活敌方目标；不存在时返回 nullopt。
    [[nodiscard]] std::optional<game::battle::BattleUnitId> selectDefaultTarget(game::battle::BattleSide actor_side) const;

    /// @brief 发送战斗结束事件并请求弹出战斗场景。
    void requestBattleEnd();
};

} // namespace game::scene
