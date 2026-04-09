#include "game/battle/battle_ai_planner.h"

#include "game/data/rpg_catalog.h"
#include "game/data/rpg_data.h"
#include "game/data/rpg_types.h"

#include <algorithm>
#include <cstdint>
#include <optional>

namespace game::battle {
namespace {

enum class ResourceKind : std::uint8_t {
    Hp = 0,
    Mp = 1
};

struct RecoveryIntent {
    bool hp{false};
    bool mp{false};

    [[nodiscard]] bool any() const { return hp || mp; }
};

struct ResourceMetric {
    ResourceKind kind{ResourceKind::Hp};
    int current{0};
    int maximum{1};
};

[[nodiscard]] constexpr BattleSide opposingSide(const BattleSide side) {
    return side == BattleSide::Player ? BattleSide::Enemy : BattleSide::Player;
}

[[nodiscard]] int safeMaximum(const int value) {
    return std::max(1, value);
}

[[nodiscard]] bool ratioLess(const int lhs_current, const int lhs_maximum, const int rhs_current, const int rhs_maximum) {
    return static_cast<std::int64_t>(lhs_current) * static_cast<std::int64_t>(safeMaximum(rhs_maximum)) <
        static_cast<std::int64_t>(rhs_current) * static_cast<std::int64_t>(safeMaximum(lhs_maximum));
}

[[nodiscard]] bool ratioEqual(const int lhs_current, const int lhs_maximum, const int rhs_current, const int rhs_maximum) {
    return static_cast<std::int64_t>(lhs_current) * static_cast<std::int64_t>(safeMaximum(rhs_maximum)) ==
        static_cast<std::int64_t>(rhs_current) * static_cast<std::int64_t>(safeMaximum(lhs_maximum));
}

[[nodiscard]] RecoveryIntent detectRecoveryIntent(const game::data::SkillData& skill) {
    RecoveryIntent intent{};
    intent.hp = skill.damage_.type == game::data::DamageType::HpRecover;
    intent.mp = skill.damage_.type == game::data::DamageType::MpRecover;

    for (const auto& effect : skill.effects_) {
        if (effect.type == game::data::EffectType::RecoverHp) {
            intent.hp = true;
        } else if (effect.type == game::data::EffectType::RecoverMp) {
            intent.mp = true;
        }
    }

    return intent;
}

[[nodiscard]] bool hasRelevantRecoveryDeficit(const BattleUnit& unit, const RecoveryIntent& intent) {
    const bool hp_missing = intent.hp && unit.hp < unit.max_hp;
    const bool mp_missing = intent.mp && unit.mp < unit.max_mp;
    return hp_missing || mp_missing;
}

[[nodiscard]] ResourceMetric hpMetric(const BattleUnit& unit) {
    return ResourceMetric{
        .kind = ResourceKind::Hp,
        .current = unit.hp,
        .maximum = safeMaximum(unit.max_hp)};
}

[[nodiscard]] ResourceMetric mpMetric(const BattleUnit& unit) {
    return ResourceMetric{
        .kind = ResourceKind::Mp,
        .current = unit.mp,
        .maximum = safeMaximum(unit.max_mp)};
}

[[nodiscard]] ResourceMetric selectPrimaryRecoveryMetric(const BattleUnit& unit, const RecoveryIntent& intent) {
    const bool hp_missing = intent.hp && unit.hp < unit.max_hp;
    const bool mp_missing = intent.mp && unit.mp < unit.max_mp;

    if (hp_missing && !mp_missing) {
        return hpMetric(unit);
    }
    if (mp_missing && !hp_missing) {
        return mpMetric(unit);
    }
    if (hp_missing && mp_missing) {
        const ResourceMetric hp = hpMetric(unit);
        const ResourceMetric mp = mpMetric(unit);
        if (ratioLess(mp.current, mp.maximum, hp.current, hp.maximum)) {
            return mp;
        }
        if (ratioEqual(mp.current, mp.maximum, hp.current, hp.maximum) && mp.current < hp.current) {
            return mp;
        }
        return hp;
    }

    return hpMetric(unit);
}

[[nodiscard]] bool isBetterLowHpTarget(const BattleUnit& candidate, const BattleUnit& current_best) {
    if (ratioLess(candidate.hp, candidate.max_hp, current_best.hp, current_best.max_hp)) {
        return true;
    }
    if (ratioLess(current_best.hp, current_best.max_hp, candidate.hp, candidate.max_hp)) {
        return false;
    }
    if (candidate.hp != current_best.hp) {
        return candidate.hp < current_best.hp;
    }
    return candidate.id < current_best.id;
}

[[nodiscard]] bool isBetterRecoveryTarget(const BattleUnit& candidate,
                                          const BattleUnit& current_best,
                                          const RecoveryIntent& intent) {
    const ResourceMetric candidate_metric = selectPrimaryRecoveryMetric(candidate, intent);
    const ResourceMetric best_metric = selectPrimaryRecoveryMetric(current_best, intent);
    if (ratioLess(candidate_metric.current, candidate_metric.maximum, best_metric.current, best_metric.maximum)) {
        return true;
    }
    if (ratioLess(best_metric.current, best_metric.maximum, candidate_metric.current, candidate_metric.maximum)) {
        return false;
    }
    if (candidate_metric.current != best_metric.current) {
        return candidate_metric.current < best_metric.current;
    }
    if (candidate_metric.kind != best_metric.kind) {
        return candidate_metric.kind == ResourceKind::Hp;
    }
    return candidate.id < current_best.id;
}

[[nodiscard]] std::optional<BattleUnitId> chooseLowestHpTarget(const std::vector<BattleUnit>& units, const BattleSide side) {
    const BattleUnit* best = nullptr;
    for (const auto& unit : units) {
        if (unit.side != side || !unit.isAlive()) {
            continue;
        }
        if (!best || isBetterLowHpTarget(unit, *best)) {
            best = &unit;
        }
    }
    return best ? std::optional<BattleUnitId>{best->id} : std::nullopt;
}

[[nodiscard]] std::optional<BattleUnitId> chooseRecoveryTarget(const std::vector<BattleUnit>& units,
                                                               const BattleSide side,
                                                               const RecoveryIntent& intent) {
    const BattleUnit* best = nullptr;
    for (const auto& unit : units) {
        if (unit.side != side || !unit.isAlive() || !hasRelevantRecoveryDeficit(unit, intent)) {
            continue;
        }
        if (!best || isBetterRecoveryTarget(unit, *best, intent)) {
            best = &unit;
        }
    }
    return best ? std::optional<BattleUnitId>{best->id} : std::nullopt;
}

[[nodiscard]] bool hasAliveUnitOnSide(const std::vector<BattleUnit>& units, const BattleSide side) {
    return std::any_of(units.begin(), units.end(), [side](const BattleUnit& unit) {
        return unit.side == side && unit.isAlive();
    });
}

[[nodiscard]] bool hasRecoveryTargetOnSide(const std::vector<BattleUnit>& units,
                                           const BattleSide side,
                                           const RecoveryIntent& intent) {
    return std::any_of(units.begin(), units.end(), [side, &intent](const BattleUnit& unit) {
        return unit.side == side && unit.isAlive() && hasRelevantRecoveryDeficit(unit, intent);
    });
}

[[nodiscard]] std::optional<BattleAction> buildSkillAction(const BattleUnit& actor,
                                                           const game::data::SkillData& skill,
                                                           const std::vector<BattleUnit>& units) {
    const RecoveryIntent recovery_intent = detectRecoveryIntent(skill);
    switch (skill.scope_) {
        case game::data::Scope::OneEnemy: {
            const auto target_id = chooseLowestHpTarget(units, opposingSide(actor.side));
            if (!target_id.has_value()) {
                return std::nullopt;
            }
            return BattleAction{
                .type = BattleActionType::Skill,
                .actor_id = actor.id,
                .target_id = target_id,
                .skill_id = skill.id_};
        }
        case game::data::Scope::AllEnemies:
            if (!hasAliveUnitOnSide(units, opposingSide(actor.side))) {
                return std::nullopt;
            }
            return BattleAction{
                .type = BattleActionType::Skill,
                .actor_id = actor.id,
                .target_id = std::nullopt,
                .skill_id = skill.id_};
        case game::data::Scope::OneAlly: {
            const auto target_id = recovery_intent.any()
                ? chooseRecoveryTarget(units, actor.side, recovery_intent)
                : chooseLowestHpTarget(units, actor.side);
            if (!target_id.has_value()) {
                return std::nullopt;
            }
            return BattleAction{
                .type = BattleActionType::Skill,
                .actor_id = actor.id,
                .target_id = target_id,
                .skill_id = skill.id_};
        }
        case game::data::Scope::AllAllies:
            if (!hasAliveUnitOnSide(units, actor.side)) {
                return std::nullopt;
            }
            if (recovery_intent.any() && !hasRecoveryTargetOnSide(units, actor.side, recovery_intent)) {
                return std::nullopt;
            }
            return BattleAction{
                .type = BattleActionType::Skill,
                .actor_id = actor.id,
                .target_id = std::nullopt,
                .skill_id = skill.id_};
        case game::data::Scope::Self:
            if (!actor.isAlive()) {
                return std::nullopt;
            }
            if (recovery_intent.any() && !hasRelevantRecoveryDeficit(actor, recovery_intent)) {
                return std::nullopt;
            }
            return BattleAction{
                .type = BattleActionType::Skill,
                .actor_id = actor.id,
                .target_id = std::nullopt,
                .skill_id = skill.id_};
        case game::data::Scope::None:
            return std::nullopt;
    }

    return std::nullopt;
}

} // namespace

BattleAction BattleAiPlanner::planEnemyAction(const BattleUnit& actor,
                                              const game::data::EnemyData& enemy,
                                              const std::vector<BattleUnit>& units,
                                              const game::data::RpgCatalog& rpg_catalog) {
    const game::data::EnemyActionData* best_action = nullptr;
    std::optional<BattleAction> best_planned_action{};
    int best_rating = 0;

    for (const auto& enemy_action : enemy.actions_) {
        if (enemy_action.skill_id_.empty()) {
            continue;
        }

        const auto* skill = rpg_catalog.findSkill(enemy_action.skill_id_);
        if (!skill || skill->scope_ == game::data::Scope::None || actor.mp < skill->mp_cost_) {
            continue;
        }

        const auto planned_action = buildSkillAction(actor, *skill, units);
        if (!planned_action.has_value()) {
            continue;
        }

        if (!best_planned_action.has_value() || enemy_action.rating_ > best_rating) {
            best_action = &enemy_action;
            best_planned_action = planned_action;
            best_rating = enemy_action.rating_;
        }
    }

    if (best_action && best_planned_action.has_value()) {
        return *best_planned_action;
    }

    return planFallbackAction(actor, units);
}

BattleAction BattleAiPlanner::planFallbackAction(const BattleUnit& actor, const std::vector<BattleUnit>& units) {
    const auto target_id = chooseLowestHpTarget(units, opposingSide(actor.side));
    if (!target_id.has_value()) {
        return BattleAction{
            .type = BattleActionType::EndTurn,
            .actor_id = actor.id,
            .target_id = std::nullopt};
    }

    return BattleAction{
        .type = BattleActionType::Attack,
        .actor_id = actor.id,
        .target_id = target_id};
}

} // namespace game::battle
