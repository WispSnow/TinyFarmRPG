#include "game/battle/battle_action_resolver.h"

#include "game/battle/turn_core.h"
#include "game/data/item_catalog.h"
#include "game/data/rpg_catalog.h"
#include "game/data/rpg_data.h"
#include "game/data/rpg_types.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

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

[[nodiscard]] int clampPercent(const int value) {
    return std::clamp(value, 0, 100);
}

void appendUnique(std::vector<std::string>& out_values, const std::string& value) {
    if (value.empty()) {
        return;
    }
    if (std::find(out_values.begin(), out_values.end(), value) == out_values.end()) {
        out_values.push_back(value);
    }
}

[[nodiscard]] int effectChancePercent(const game::data::EffectData& effect) {
    if (effect.value1 <= 0.0F) {
        return 100;
    }
    if (effect.value1 <= 1.0F) {
        return clampPercent(static_cast<int>(std::lround(effect.value1 * 100.0F)));
    }
    return clampPercent(static_cast<int>(std::lround(effect.value1)));
}

[[nodiscard]] int effectFlatValue(const game::data::EffectData& effect) {
    if (effect.count > 0) {
        return effect.count;
    }
    return static_cast<int>(std::lround(effect.value1));
}

} // namespace

BattleActionResolver::BattleActionResolver(EscapeRollFunc escape_roll_override)
    : escape_roll_override_(std::move(escape_roll_override)) {
}

BattleActionResolver::BattleActionResolver(Dependencies dependencies, EscapeRollFunc escape_roll_override)
    : dependencies_(dependencies),
      escape_roll_override_(std::move(escape_roll_override)) {
}

int BattleActionResolver::nextPercentRoll() {
    if (escape_roll_override_) {
        return escape_roll_override_();
    }
    std::uniform_int_distribution<int> distribution(1, 100);
    return distribution(random_engine_);
}

int BattleActionResolver::nextEscapeRoll() {
    return nextPercentRoll();
}

bool BattleActionResolver::collectTargets(const BattleAction& action,
                                          const BattleUnit& actor,
                                          const game::data::Scope scope,
                                          TurnCore& turn_core,
                                          std::vector<BattleUnit*>& out_targets,
                                          std::string& out_error,
                                          const std::string_view action_label) {
    out_targets.clear();
    out_error.clear();

    const auto make_error = [action_label](const std::string_view reason) {
        return std::string{action_label} + " " + std::string{reason};
    };

    const auto first_match = [&turn_core](auto&& predicate) -> BattleUnit* {
        for (const auto unit_id : turn_core.turnOrder()) {
            BattleUnit* candidate = turn_core.findUnitMutable(unit_id);
            if (!candidate || !candidate->isAlive()) {
                continue;
            }
            if (predicate(*candidate)) {
                return candidate;
            }
        }
        return nullptr;
    };

    const auto validate_target = [&actor](const BattleUnit* unit, const game::data::Scope scope) -> bool {
        if (!unit || !unit->isAlive()) {
            return false;
        }

        switch (scope) {
            case game::data::Scope::OneEnemy:
                return unit->side != actor.side;
            case game::data::Scope::OneAlly:
                return unit->side == actor.side;
            case game::data::Scope::Self:
                return unit->id == actor.id;
            default:
                return false;
        }
    };

    const auto push_single_target = [&out_targets](BattleUnit* unit) {
        if (unit) {
            out_targets.push_back(unit);
        }
    };

    switch (scope) {
        case game::data::Scope::Self: {
            push_single_target(turn_core.findUnitMutable(actor.id));
            break;
        }
        case game::data::Scope::OneEnemy: {
            BattleUnit* target = nullptr;
            if (action.target_id.has_value()) {
                target = turn_core.findUnitMutable(*action.target_id);
            } else {
                target = first_match([&actor](const BattleUnit& unit) {
                    return unit.side != actor.side;
                });
            }

            if (!validate_target(target, game::data::Scope::OneEnemy)) {
                out_error = make_error("target is invalid");
                return false;
            }
            push_single_target(target);
            break;
        }
        case game::data::Scope::OneAlly: {
            BattleUnit* target = nullptr;
            if (action.target_id.has_value()) {
                target = turn_core.findUnitMutable(*action.target_id);
            } else {
                target = turn_core.findUnitMutable(actor.id);
            }

            if (!validate_target(target, game::data::Scope::OneAlly)) {
                out_error = make_error("target is invalid");
                return false;
            }
            push_single_target(target);
            break;
        }
        case game::data::Scope::AllEnemies: {
            for (const auto unit_id : turn_core.turnOrder()) {
                BattleUnit* target = turn_core.findUnitMutable(unit_id);
                if (!target || !target->isAlive()) {
                    continue;
                }
                if (target->side != actor.side) {
                    out_targets.push_back(target);
                }
            }
            break;
        }
        case game::data::Scope::AllAllies: {
            for (const auto unit_id : turn_core.turnOrder()) {
                BattleUnit* target = turn_core.findUnitMutable(unit_id);
                if (!target || !target->isAlive()) {
                    continue;
                }
                if (target->side == actor.side) {
                    out_targets.push_back(target);
                }
            }
            break;
        }
        case game::data::Scope::None:
            out_error = make_error("scope is none");
            return false;
    }

    if (out_targets.empty()) {
        out_error = make_error("has no valid targets");
        return false;
    }

    return true;
}

bool BattleActionResolver::collectSkillTargets(const BattleAction& action,
                                               const BattleUnit& actor,
                                               const game::data::SkillData& skill,
                                               TurnCore& turn_core,
                                               std::vector<BattleUnit*>& out_targets,
                                               std::string& out_error) {
    return collectTargets(action, actor, skill.scope_, turn_core, out_targets, out_error, "skill");
}

void BattleActionResolver::applySkillEffects(const game::data::SkillData& skill,
                                             BattleUnit& target,
                                             BattleRuntimeState::UnitRuntimeState& target_state,
                                             BattleActionResult& result) {
    for (const auto& effect : skill.effects_) {
        switch (effect.type) {
            case game::data::EffectType::RecoverHp: {
                const int recover = std::max(0, effectFlatValue(effect));
                if (recover <= 0) {
                    break;
                }
                const int before = target.hp;
                target.hp = std::min(target.max_hp, target.hp + recover);
                result.hp_recovered += target.hp - before;
                break;
            }
            case game::data::EffectType::RecoverMp: {
                const int recover = std::max(0, effectFlatValue(effect));
                if (recover <= 0) {
                    break;
                }
                const int before = target.mp;
                target.mp = std::min(target.max_mp, target.mp + recover);
                result.mp_recovered += target.mp - before;
                break;
            }
            case game::data::EffectType::AddState: {
                if (effect.target_id.empty()) {
                    break;
                }

                const int chance_percent = effectChancePercent(effect);
                if (nextPercentRoll() > chance_percent) {
                    break;
                }

                int turns = 1;
                if (dependencies_.rpg_catalog) {
                    if (const auto* state = dependencies_.rpg_catalog->findState(effect.target_id)) {
                        turns = std::max(1, state->min_turns_);
                    }
                }

                int& turns_left = target_state.state_turns_left[effect.target_id];
                turns_left = std::max(turns_left, turns);
                appendUnique(result.states_added, effect.target_id);
                break;
            }
            case game::data::EffectType::RemoveState: {
                if (effect.target_id.empty()) {
                    break;
                }

                if (target_state.state_turns_left.erase(effect.target_id) > 0) {
                    appendUnique(result.states_removed, effect.target_id);
                }
                break;
            }
            case game::data::EffectType::AddItem:
            case game::data::EffectType::Unknown:
                break;
        }
    }
}

void BattleActionResolver::applyBattleItemEffects(const game::data::BattleItemUseConfig& use,
                                                  BattleUnit& target,
                                                  BattleActionResult& result) {
    for (const auto& effect : use.effects) {
        switch (effect.type) {
            case game::data::BattleItemEffectType::RecoverHp: {
                const int before = target.hp;
                target.hp = std::min(target.max_hp, target.hp + effect.amount);
                result.hp_recovered += target.hp - before;
                break;
            }
            case game::data::BattleItemEffectType::RecoverMp: {
                const int before = target.mp;
                target.mp = std::min(target.max_mp, target.mp + effect.amount);
                result.mp_recovered += target.mp - before;
                break;
            }
            case game::data::BattleItemEffectType::Unknown:
                break;
        }
    }
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

    BattleUnit* actor = turn_core.findUnitMutable(action.actor_id);
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
            if (!dependencies_.rpg_catalog) {
                result.failure_reason = "rpg catalog is unavailable";
                return result;
            }
            if (action.skill_id.empty()) {
                result.failure_reason = "skill id is missing";
                return result;
            }

            const game::data::SkillData* skill = dependencies_.rpg_catalog->findSkill(action.skill_id);
            if (!skill) {
                result.failure_reason = "skill does not exist";
                return result;
            }

            std::vector<BattleUnit*> targets{};
            if (!collectSkillTargets(action, *actor, *skill, turn_core, targets, result.failure_reason)) {
                return result;
            }
            if (actor->mp < skill->mp_cost_) {
                result.failure_reason = "insufficient mp";
                return result;
            }

            result.status = BattleActionStatus::Applied;
            result.mp_spent = skill->mp_cost_;
            actor->mp = std::max(0, actor->mp - skill->mp_cost_);
            if (nextPercentRoll() > clampPercent(skill->success_rate_)) {
                result.missed = true;
                (void)turn_core.advanceTurn();
                return result;
            }

            for (int repeat = 0; repeat < skill->repeats_; ++repeat) {
                for (BattleUnit* target : targets) {
                    if (!target || !target->isAlive()) {
                        continue;
                    }

                    auto& target_state = runtime_state.units.try_emplace(target->id).first->second;

                    int evaluated_damage = 0;
                    std::string eval_error{};
                    const bool eval_ok = formula_evaluator_.evaluate(skill->damage_, *actor, *target, evaluated_damage, eval_error);
                    const int base_value = eval_ok ? evaluated_damage : 0;

                    switch (skill->damage_.type) {
                        case game::data::DamageType::HpDamage:
                        case game::data::DamageType::HpDrain: {
                            int damage = std::max(0, base_value);
                            if (damage > 0 && target_state.guarding) {
                                result.target_guarded = true;
                                damage = std::max(1, damage / 2);
                            }

                            if (damage > 0) {
                                const int hp_before = target->hp;
                                target->hp = std::max(0, target->hp - damage);
                                const int actual_damage = hp_before - target->hp;
                                result.damage += actual_damage;
                                if (skill->damage_.type == game::data::DamageType::HpDrain && actual_damage > 0) {
                                    const int actor_before = actor->hp;
                                    actor->hp = std::min(actor->max_hp, actor->hp + actual_damage);
                                    result.hp_recovered += actor->hp - actor_before;
                                }
                            }
                            break;
                        }
                        case game::data::DamageType::MpDamage:
                        case game::data::DamageType::MpDrain: {
                            const int damage = std::max(0, base_value);
                            if (damage > 0) {
                                const int mp_before = target->mp;
                                target->mp = std::max(0, target->mp - damage);
                                const int actual_damage = mp_before - target->mp;
                                if (skill->damage_.type == game::data::DamageType::MpDrain && actual_damage > 0) {
                                    const int actor_before = actor->mp;
                                    actor->mp = std::min(actor->max_mp, actor->mp + actual_damage);
                                    result.mp_recovered += actor->mp - actor_before;
                                }
                            }
                            break;
                        }
                        case game::data::DamageType::HpRecover: {
                            const int recover = std::max(0, base_value);
                            if (recover > 0) {
                                const int hp_before = target->hp;
                                target->hp = std::min(target->max_hp, target->hp + recover);
                                result.hp_recovered += target->hp - hp_before;
                            }
                            break;
                        }
                        case game::data::DamageType::MpRecover: {
                            const int recover = std::max(0, base_value);
                            if (recover > 0) {
                                const int mp_before = target->mp;
                                target->mp = std::min(target->max_mp, target->mp + recover);
                                result.mp_recovered += target->mp - mp_before;
                            }
                            break;
                        }
                        case game::data::DamageType::None:
                            break;
                    }

                    applySkillEffects(*skill, *target, target_state, result);
                    result.target_defeated = result.target_defeated || !target->isAlive();
                }
            }

            turn_core.refresh();
            if (turn_core.outcome() == BattleOutcome::Ongoing) {
                (void)turn_core.advanceTurn();
            }
            return result;
        }
        case BattleActionType::Item: {
            if (!dependencies_.item_catalog) {
                result.failure_reason = "item catalog is unavailable";
                return result;
            }
            if (action.item_id.empty()) {
                result.failure_reason = "item id is missing";
                return result;
            }

            const entt::id_type item_id = game::data::RpgCatalog::hashId(action.item_id);
            const auto* item = dependencies_.item_catalog->findItem(item_id);
            if (!item) {
                result.failure_reason = "item does not exist";
                return result;
            }
            if (!item->battle_use_) {
                result.failure_reason = "item cannot be used in battle";
                return result;
            }

            const auto& battle_use = *item->battle_use_;
            auto stock_it = runtime_state.item_stocks.find(item_id);
            const int consume = std::max(1, battle_use.consume);
            if (stock_it == runtime_state.item_stocks.end() || stock_it->second < consume) {
                result.failure_reason = "insufficient item stock";
                return result;
            }

            if (battle_use.effects.empty()) {
                result.failure_reason = "item has no battle effects";
                return result;
            }

            std::vector<BattleUnit*> targets{};
            if (!collectTargets(action, *actor, battle_use.scope, turn_core, targets, result.failure_reason, "item")) {
                return result;
            }

            result.status = BattleActionStatus::Applied;
            for (BattleUnit* target : targets) {
                if (!target || !target->isAlive()) {
                    continue;
                }
                applyBattleItemEffects(battle_use, *target, result);
            }

            stock_it->second -= consume;
            if (stock_it->second <= 0) {
                runtime_state.item_stocks.erase(stock_it);
            }

            turn_core.refresh();
            if (turn_core.outcome() == BattleOutcome::Ongoing) {
                (void)turn_core.advanceTurn();
            }
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
