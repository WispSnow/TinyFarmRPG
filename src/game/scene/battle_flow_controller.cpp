#include "game/scene/battle_flow_controller.h"

namespace game::scene {

void BattleFlowController::run(const float delta_time, Delegate& delegate) {
    bool keep_running = true;
    while (keep_running) {
        keep_running = false;

        switch (state_) {
            case BattleFlowState::WaitingForInput:
                return;
            case BattleFlowState::ExecutingAction:
                if (!delegate.hasPendingAction()) {
                    delegate.beginCurrentTurnFlow();
                    keep_running = true;
                    break;
                }

                delegate.executePendingAction();
                delegate.leaveInputMenu();
                state_ = BattleFlowState::AnimatingResult;
                keep_running = true;
                break;
            case BattleFlowState::AnimatingResult:
                delegate.updateResultAnimation(delta_time);
                if (delegate.resultAnimationFinished()) {
                    state_ = BattleFlowState::CheckVictory;
                    keep_running = true;
                }
                break;
            case BattleFlowState::CheckVictory: {
                const game::battle::BattleOutcome outcome = delegate.battleOutcome();
                if (outcome == game::battle::BattleOutcome::Ongoing) {
                    state_ = BattleFlowState::NextTurn;
                } else if (outcome == game::battle::BattleOutcome::Victory) {
                    startVictoryFlow(delegate);
                } else if (outcome == game::battle::BattleOutcome::Defeat) {
                    startDefeatFlow(delegate);
                } else {
                    state_ = BattleFlowState::BattleEnd;
                }
                keep_running = true;
                break;
            }
            case BattleFlowState::VictoryFlow:
                delegate.updateVictoryFlow(delta_time);
                if (delegate.victoryFlowFinished()) {
                    delegate.finishVictoryFlow();
                    state_ = BattleFlowState::BattleEnd;
                    keep_running = true;
                }
                break;
            case BattleFlowState::DefeatFlow:
                delegate.updateDefeatFlow(delta_time);
                if (delegate.defeatFlowFinished()) {
                    delegate.finishDefeatFlow();
                    state_ = BattleFlowState::BattleEnd;
                    keep_running = true;
                }
                break;
            case BattleFlowState::NextTurn:
                delegate.beginCurrentTurnFlow();
                keep_running = true;
                break;
            case BattleFlowState::BattleEnd:
                delegate.leaveInputMenu();
                delegate.requestBattleEnd();
                return;
        }
    }
}

void BattleFlowController::startVictoryFlow(Delegate& delegate) {
    state_ = BattleFlowState::VictoryFlow;
    delegate.beginVictoryFlow();
}

void BattleFlowController::startDefeatFlow(Delegate& delegate) {
    state_ = BattleFlowState::DefeatFlow;
    delegate.beginDefeatFlow();
}

void BattleFlowController::waitForInput() {
    state_ = BattleFlowState::WaitingForInput;
}

void BattleFlowController::beginExecutingAction() {
    state_ = BattleFlowState::ExecutingAction;
}

void BattleFlowController::setBattleEnd() {
    state_ = BattleFlowState::BattleEnd;
}

} // namespace game::scene
