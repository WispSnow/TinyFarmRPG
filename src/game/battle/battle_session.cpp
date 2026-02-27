#include "battle_session.h"

#include <algorithm>

namespace game::battle {

BattleSession::BattleSession(std::vector<BattleUnit> units)
    : turn_core_(std::move(units)) {
}

BattleSnapshot BattleSession::snapshot() const {
    BattleSnapshot snapshot{};
    snapshot.units = turn_core_.units();
    snapshot.current_actor_id = turn_core_.currentActorId();
    snapshot.outcome = turn_core_.outcome();
    return snapshot;
}

BattleActionResult BattleSession::submitAction(const BattleAction& action) {
    BattleActionResult result = makeRejectedResult(action);

    if (turn_core_.outcome() != BattleOutcome::Ongoing) {
        fillSnapshot(result);
        return result;
    }

    const auto current_actor = turn_core_.currentActorId();
    if (!current_actor || *current_actor != action.actor_id) {
        fillSnapshot(result);
        return result;
    }

    const BattleUnit* actor = turn_core_.findUnit(action.actor_id);
    if (!actor || !actor->isAlive()) {
        fillSnapshot(result);
        return result;
    }

    switch (action.type) {
        case BattleActionType::EndTurn: {
            result.status = BattleActionStatus::Applied;
            (void)turn_core_.advanceTurn();
            break;
        }
        case BattleActionType::Skill:
        case BattleActionType::Item:
        case BattleActionType::Guard:
        case BattleActionType::Escape: {
            // Phase 4: 动作语义将在 battle_action_resolver 中实现。
            // 当前显式返回 Rejected，避免误判为已应用动作。
            result.status = BattleActionStatus::Rejected;
            fillSnapshot(result);
            return result;
        }
        case BattleActionType::Attack: {
            if (!action.target_id) {
                fillSnapshot(result);
                return result;
            }

            BattleUnit* target = turn_core_.findUnitMutable(*action.target_id);
            if (!target || !target->isAlive() || target->side == actor->side) {
                fillSnapshot(result);
                return result;
            }

            const int damage = std::max(1, actor->attack);
            target->hp = std::max(0, target->hp - damage);

            result.status = BattleActionStatus::Applied;
            result.damage = damage;
            result.target_defeated = !target->isAlive();

            turn_core_.refresh();
            // TODO(FND-010): advanceTurn() 目前会再次 evaluateOutcome()，后续可收敛重复判定。
            if (turn_core_.outcome() == BattleOutcome::Ongoing) {
                (void)turn_core_.advanceTurn();
            }
            break;
        }
    }

    fillSnapshot(result);
    return result;
}

BattleActionResult BattleSession::makeRejectedResult(const BattleAction& action) const {
    BattleActionResult result{};
    result.status = BattleActionStatus::Rejected;
    result.action_type = action.type;
    result.actor_id = action.actor_id;
    result.target_id = action.target_id;
    result.damage = 0;
    result.target_defeated = false;
    return result;
}

void BattleSession::fillSnapshot(BattleActionResult& result) const {
    result.snapshot = snapshot();
    result.outcome_after = result.snapshot.outcome;
}

} // namespace game::battle
