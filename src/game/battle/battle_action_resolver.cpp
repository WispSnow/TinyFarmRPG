#include "game/battle/battle_action_resolver.h"

#include "game/battle/turn_core.h"

#include <algorithm>
#include <string>
#include <utility>

namespace game::battle {
namespace {

constexpr int kEscapeSuccessChancePercent = 50;

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

BattleActionResolver::BattleActionResolver(EscapeRollFunc escape_roll_override)
    : escape_roll_override_(std::move(escape_roll_override)) {
}

int BattleActionResolver::nextEscapeRoll() {
    if (escape_roll_override_) {
        return escape_roll_override_();
    }
    std::uniform_int_distribution<int> distribution(1, 100);
    return distribution(random_engine_);
}

BattleActionResult BattleActionResolver::resolve(const BattleAction& action,
                                                 TurnCore& turn_core,
                                                 BattleRuntimeState& runtime_state) {
    BattleActionResult result = makeRejectedResult(action);

    if (turn_core.outcome() != BattleOutcome::Ongoing) {
        result.failure_reason = "battle is not ongoing";
        return result;
    }

    const auto current_actor = turn_core.currentActorId();
    if (!current_actor || *current_actor != action.actor_id) {
        result.failure_reason = "actor is not current turn owner";
        return result;
    }

    const BattleUnit* actor = turn_core.findUnit(action.actor_id);
    if (!actor || !actor->isAlive()) {
        result.failure_reason = "actor is missing or defeated";
        return result;
    }

    auto& actor_state = runtime_state.units.try_emplace(action.actor_id).first->second;

    switch (action.type) {
        case BattleActionType::EndTurn: {
            result.status = BattleActionStatus::Applied;
            (void)turn_core.advanceTurn();
            return result;
        }
        case BattleActionType::Guard:
            actor_state.guarding = true;
            result.status = BattleActionStatus::Applied;
            (void)turn_core.advanceTurn();
            return result;
        case BattleActionType::Escape: {
            result.status = BattleActionStatus::Applied;
            ++runtime_state.escape_attempt_count;
            const int roll = nextEscapeRoll();
            result.escape_succeeded = roll <= kEscapeSuccessChancePercent;
            if (result.escape_succeeded) {
                turn_core.forceOutcome(BattleOutcome::Escaped);
            } else {
                (void)turn_core.advanceTurn();
            }
            return result;
        }
        case BattleActionType::Skill: {
            result.failure_reason = "skill action is not implemented yet";
            return result;
        }
        case BattleActionType::Item: {
            result.failure_reason = "item action is not implemented yet";
            return result;
        }
        case BattleActionType::Attack: {
            if (!action.target_id) {
                result.failure_reason = "attack target is missing";
                return result;
            }

            BattleUnit* target = turn_core.findUnitMutable(*action.target_id);
            if (!target || !target->isAlive() || target->side == actor->side) {
                result.failure_reason = "attack target is invalid";
                return result;
            }

            int evaluated_damage = 0;
            std::string eval_error{};
            const bool eval_ok = formula_evaluator_.evaluate("a.atk", *actor, *target, evaluated_damage, eval_error);
            int damage = eval_ok ? std::max(1, evaluated_damage) : std::max(1, actor->attack);

            auto& target_state = runtime_state.units.try_emplace(target->id).first->second;
            if (target_state.guarding) {
                result.target_guarded = true;
                damage = std::max(1, damage / 2);
            }

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
