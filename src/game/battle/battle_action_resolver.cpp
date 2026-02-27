#include "game/battle/battle_action_resolver.h"

#include "game/battle/turn_core.h"

#include <algorithm>
#include <string>

namespace game::battle {
namespace {

BattleActionResult makeRejectedResult(const BattleAction& action) {
    BattleActionResult result{};
    result.status = BattleActionStatus::Rejected;
    result.action_type = action.type;
    result.actor_id = action.actor_id;
    result.target_id = action.target_id;
    result.damage = 0;
    result.target_defeated = false;
    return result;
}

} // namespace

BattleActionResult BattleActionResolver::resolve(const BattleAction& action, TurnCore& turn_core) {
    BattleActionResult result = makeRejectedResult(action);

    if (turn_core.outcome() != BattleOutcome::Ongoing) {
        return result;
    }

    const auto current_actor = turn_core.currentActorId();
    if (!current_actor || *current_actor != action.actor_id) {
        return result;
    }

    const BattleUnit* actor = turn_core.findUnit(action.actor_id);
    if (!actor || !actor->isAlive()) {
        return result;
    }

    switch (action.type) {
        case BattleActionType::EndTurn: {
            result.status = BattleActionStatus::Applied;
            (void)turn_core.advanceTurn();
            return result;
        }
        case BattleActionType::Skill:
        case BattleActionType::Item:
        case BattleActionType::Guard:
        case BattleActionType::Escape: {
            // Phase 4: 分类型语义将逐步接入。
            result.status = BattleActionStatus::Rejected;
            return result;
        }
        case BattleActionType::Attack: {
            if (!action.target_id) {
                return result;
            }

            BattleUnit* target = turn_core.findUnitMutable(*action.target_id);
            if (!target || !target->isAlive() || target->side == actor->side) {
                return result;
            }

            int evaluated_damage = 0;
            std::string eval_error{};
            const bool eval_ok = formula_evaluator_.evaluate("a.atk", *actor, *target, evaluated_damage, eval_error);
            const int damage = eval_ok ? std::max(1, evaluated_damage) : std::max(1, actor->attack);
            target->hp = std::max(0, target->hp - damage);

            result.status = BattleActionStatus::Applied;
            result.damage = damage;
            result.target_defeated = !target->isAlive();

            turn_core.refresh();
            if (turn_core.outcome() == BattleOutcome::Ongoing) {
                (void)turn_core.advanceTurn();
            }
            return result;
        }
    }

    return result;
}

} // namespace game::battle
