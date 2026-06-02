#pragma once

#include "game/battle/battle_types.h"
#include "game/scene/battle_scene_state.h"

namespace game::scene {

/// @brief 推进 BattleScene 的同步战斗流程状态机。
class BattleFlowController final {
public:
    class Delegate {
    public:
        virtual ~Delegate() = default;

        [[nodiscard]] virtual bool hasPendingAction() const = 0;
        virtual void executePendingAction() = 0;
        virtual void beginCurrentTurnFlow() = 0;
        virtual void updateResultAnimation(float delta_time) = 0;
        [[nodiscard]] virtual bool resultAnimationFinished() const = 0;
        [[nodiscard]] virtual game::battle::BattleOutcome battleOutcome() const = 0;
        virtual void beginVictoryFlow() = 0;
        virtual void updateVictoryFlow(float delta_time) = 0;
        [[nodiscard]] virtual bool victoryFlowFinished() const = 0;
        virtual void finishVictoryFlow() = 0;
        virtual void beginDefeatFlow() = 0;
        virtual void updateDefeatFlow(float delta_time) = 0;
        [[nodiscard]] virtual bool defeatFlowFinished() const = 0;
        virtual void finishDefeatFlow() = 0;
        virtual void leaveInputMenu() = 0;
        virtual void requestBattleEnd() = 0;
    };

    void run(float delta_time, Delegate& delegate);
    void startVictoryFlow(Delegate& delegate);
    void startDefeatFlow(Delegate& delegate);

    void waitForInput();
    void beginExecutingAction();
    void setBattleEnd();

    [[nodiscard]] BattleFlowState state() const { return state_; }
    [[nodiscard]] bool isWaitingForInput() const { return state_ == BattleFlowState::WaitingForInput; }
    [[nodiscard]] bool isVictoryFlow() const { return state_ == BattleFlowState::VictoryFlow; }
    [[nodiscard]] bool isDefeatFlow() const { return state_ == BattleFlowState::DefeatFlow; }

private:
    BattleFlowState state_{BattleFlowState::WaitingForInput};
};

} // namespace game::scene
