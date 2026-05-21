#include "game/domain/actor_progression_service.h"

#include "game/battle/actor_stats_resolver.h"
#include "game/data/rpg_catalog.h"

#include <entt/entity/registry.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace game::domain {
namespace {

[[nodiscard]] int clampToInt(const std::int64_t value) {
    return static_cast<int>(std::clamp<std::int64_t>(
        value,
        0,
        static_cast<std::int64_t>(std::numeric_limits<int>::max())));
}

[[nodiscard]] const game::data::ClassData* findClassForActor(const game::data::RpgCatalog& catalog,
                                                             const game::data::ActorData& actor) {
    return catalog.findClass(actor.class_id_);
}

[[nodiscard]] int maxLevelForActor(const game::data::ActorData& actor) {
    return std::max(actor.initial_level_, actor.max_level_);
}

[[nodiscard]] int paramValue(const game::data::ParamArray& params, const game::data::ParamIndex index) {
    return params[static_cast<std::size_t>(index)];
}

[[nodiscard]] const game::component::ActorEquipmentLoadout* findLoadout(
    const std::unordered_map<std::string, game::component::ActorEquipmentLoadout>& actor_equipment,
    const std::string_view actor_id) {
    const auto it = actor_equipment.find(std::string{actor_id});
    return it == actor_equipment.end() ? nullptr : &it->second;
}

[[nodiscard]] const game::component::ActorEquipmentLoadout* findLoadout(
    const entt::registry& registry,
    const entt::entity player,
    const std::string_view actor_id) {
    const auto* equipment = registry.try_get<game::component::PartyEquipmentComponent>(player);
    if (!equipment) {
        return nullptr;
    }
    return findLoadout(equipment->loadouts_by_actor_id_, actor_id);
}

[[nodiscard]] std::vector<std::string> uniqueActorIds(const std::vector<std::string>& actor_ids) {
    std::vector<std::string> unique{};
    unique.reserve(actor_ids.size());
    std::unordered_set<std::string> seen{};
    seen.reserve(actor_ids.size());
    for (const auto& actor_id : actor_ids) {
        if (actor_id.empty() || !seen.insert(actor_id).second) {
            continue;
        }
        unique.push_back(actor_id);
    }
    return unique;
}

} // namespace

bool PartyExperienceGrantResult::anyLevelUp() const {
    return std::any_of(actors.begin(), actors.end(), [](const ActorExperienceGrant& grant) {
        return grant.leveledUp();
    });
}

int ActorProgressionService::expForLevel(const game::data::ExpCurveData& curve, const int level) {
    if (level <= 1) {
        return 0;
    }

    const double safe_acc_b = static_cast<double>(std::max(1, curve.acc_b_));
    const double safe_level = static_cast<double>(level);
    const double previous_level = static_cast<double>(level - 1);
    const double exponent = 0.9 + static_cast<double>(std::max(0, curve.acc_a_)) / 250.0;
    const double value =
        static_cast<double>(std::max(1, curve.basis_)) * std::pow(previous_level, exponent) *
            safe_level * (safe_level + 1.0) /
            (6.0 + safe_level * safe_level / 50.0 / safe_acc_b) +
        previous_level * static_cast<double>(std::max(0, curve.extra_));
    return clampToInt(static_cast<std::int64_t>(std::llround(value)));
}

int ActorProgressionService::expForLevel(const game::data::RpgCatalog& catalog,
                                         const game::data::ActorData& actor,
                                         const int level) {
    const auto* klass = findClassForActor(catalog, actor);
    const int clamped_level = std::clamp(level, 1, maxLevelForActor(actor));
    return expForLevel(klass ? klass->exp_curve_ : game::data::ExpCurveData{}, clamped_level);
}

int ActorProgressionService::levelForExp(const game::data::RpgCatalog& catalog,
                                         const game::data::ActorData& actor,
                                         const int total_exp) {
    const int min_level = std::max(1, actor.initial_level_);
    const int max_level = maxLevelForActor(actor);
    const int clamped_exp = std::max(0, total_exp);
    int result = min_level;
    for (int level = min_level; level <= max_level; ++level) {
        if (expForLevel(catalog, actor, level) > clamped_exp) {
            break;
        }
        result = level;
    }
    return std::clamp(result, min_level, max_level);
}

int ActorProgressionService::expToNextLevel(const game::data::RpgCatalog& catalog,
                                            const game::data::ActorData& actor,
                                            const int total_exp) {
    const int level = levelForExp(catalog, actor, total_exp);
    if (level >= maxLevelForActor(actor)) {
        return 0;
    }
    return std::max(0, expForLevel(catalog, actor, level + 1) - std::max(0, total_exp));
}

game::component::ActorRuntimeState ActorProgressionService::initialState(
    const game::data::RpgCatalog& catalog,
    const game::data::ActorData& actor,
    const game::component::ActorEquipmentLoadout* loadout) {
    const int level = std::clamp(actor.initial_level_, 1, maxLevelForActor(actor));
    const auto resolved = game::battle::resolveActorStats(catalog, actor, level, loadout);
    return game::component::ActorRuntimeState{
        .current_hp = paramValue(resolved.params, game::data::ParamIndex::Mhp),
        .current_mp = paramValue(resolved.params, game::data::ParamIndex::Mmp),
        .level = level,
        .total_exp = expForLevel(catalog, actor, level),
    };
}

game::component::ActorRuntimeState ActorProgressionService::normalizeState(
    const game::data::RpgCatalog& catalog,
    const game::data::ActorData& actor,
    game::component::ActorRuntimeState state,
    const game::component::ActorEquipmentLoadout* loadout) {
    const int min_exp = expForLevel(catalog, actor, actor.initial_level_);
    const int max_exp = expForLevel(catalog, actor, maxLevelForActor(actor));
    state.total_exp = std::clamp(state.total_exp, min_exp, max_exp);
    state.level = levelForExp(catalog, actor, state.total_exp);

    const auto resolved = game::battle::resolveActorStats(catalog, actor, state.level, loadout);
    const int max_hp = paramValue(resolved.params, game::data::ParamIndex::Mhp);
    const int max_mp = paramValue(resolved.params, game::data::ParamIndex::Mmp);
    state.current_hp = std::clamp(state.current_hp, 0, max_hp);
    state.current_mp = std::clamp(state.current_mp, 0, max_mp);
    return state;
}

PartyExperienceGrantResult ActorProgressionService::previewExperience(
    const game::data::RpgCatalog& catalog,
    const std::vector<std::string>& actor_ids,
    const std::unordered_map<std::string, game::component::ActorRuntimeState>& runtime_states,
    const std::unordered_map<std::string, game::component::ActorEquipmentLoadout>& actor_equipment,
    const int exp_reward) {
    PartyExperienceGrantResult result{};
    result.exp_reward = std::max(0, exp_reward);
    if (result.exp_reward <= 0) {
        return result;
    }

    const auto unique_ids = uniqueActorIds(actor_ids);
    result.actors.reserve(unique_ids.size());
    for (const auto& actor_id : unique_ids) {
        const auto* actor = catalog.findActor(actor_id);
        if (!actor) {
            continue;
        }

        const auto* loadout = findLoadout(actor_equipment, actor_id);
        const auto runtime_it = runtime_states.find(actor_id);
        auto state = runtime_it == runtime_states.end()
            ? initialState(catalog, *actor, loadout)
            : normalizeState(catalog, *actor, runtime_it->second, loadout);

        const int old_level = state.level;
        const auto old_stats = game::battle::resolveActorStats(catalog, *actor, old_level, loadout);
        const int old_max_hp = paramValue(old_stats.params, game::data::ParamIndex::Mhp);
        const int old_max_mp = paramValue(old_stats.params, game::data::ParamIndex::Mmp);

        const int max_exp = expForLevel(catalog, *actor, maxLevelForActor(*actor));
        const std::int64_t next_total_exp_wide =
            static_cast<std::int64_t>(state.total_exp) + static_cast<std::int64_t>(result.exp_reward);
        const int next_total_exp = static_cast<int>(std::clamp<std::int64_t>(
            next_total_exp_wide,
            static_cast<std::int64_t>(state.total_exp),
            static_cast<std::int64_t>(max_exp)));
        const int gained_exp = next_total_exp - state.total_exp;
        const int new_level = levelForExp(catalog, *actor, next_total_exp);
        const auto new_stats = game::battle::resolveActorStats(catalog, *actor, new_level, loadout);
        const int new_max_hp = paramValue(new_stats.params, game::data::ParamIndex::Mhp);
        const int new_max_mp = paramValue(new_stats.params, game::data::ParamIndex::Mmp);

        result.actors.push_back(ActorExperienceGrant{
            .actor_id = actor->id_,
            .display_name = actor->display_name_.empty() ? actor->id_ : actor->display_name_,
            .gained_exp = gained_exp,
            .old_level = old_level,
            .new_level = new_level,
            .total_exp = next_total_exp,
            .exp_to_next = expToNextLevel(catalog, *actor, next_total_exp),
            .hp_max_delta = std::max(0, new_max_hp - old_max_hp),
            .mp_max_delta = std::max(0, new_max_mp - old_max_mp),
        });
    }

    return result;
}

PartyExperienceGrantResult ActorProgressionService::grantExperience(
    entt::registry& registry,
    const entt::entity player,
    const game::data::RpgCatalog& catalog,
    const std::vector<std::string>& actor_ids,
    const int exp_reward) {
    if (player == entt::null || !registry.valid(player)) {
        return {};
    }

    const auto* equipment = registry.try_get<game::component::PartyEquipmentComponent>(player);
    const std::unordered_map<std::string, game::component::ActorEquipmentLoadout> empty_equipment{};
    const auto& equipment_by_actor = equipment ? equipment->loadouts_by_actor_id_ : empty_equipment;
    auto& runtime_stats = registry.get_or_emplace<game::component::PartyRuntimeStatsComponent>(player);
    PartyExperienceGrantResult result =
        previewExperience(catalog, actor_ids, runtime_stats.states_by_actor_id_, equipment_by_actor, exp_reward);

    bool changed = false;
    for (const auto& grant : result.actors) {
        const auto* actor = catalog.findActor(grant.actor_id);
        if (!actor) {
            continue;
        }
        const auto* loadout = findLoadout(registry, player, grant.actor_id);
        auto state = runtime_stats.states_by_actor_id_.contains(grant.actor_id)
            ? normalizeState(catalog, *actor, runtime_stats.states_by_actor_id_.at(grant.actor_id), loadout)
            : initialState(catalog, *actor, loadout);

        state.total_exp = grant.total_exp;
        state.level = grant.new_level;
        const auto new_stats = game::battle::resolveActorStats(catalog, *actor, state.level, loadout);
        const int new_max_hp = paramValue(new_stats.params, game::data::ParamIndex::Mhp);
        const int new_max_mp = paramValue(new_stats.params, game::data::ParamIndex::Mmp);

        if (grant.leveledUp()) {
            state.current_hp += grant.hp_max_delta;
            state.current_mp += grant.mp_max_delta;
        }
        state.current_hp = std::clamp(state.current_hp, 0, new_max_hp);
        state.current_mp = std::clamp(state.current_mp, 0, new_max_mp);

        auto& stored = runtime_stats.states_by_actor_id_[grant.actor_id];
        if (stored.current_hp != state.current_hp ||
            stored.current_mp != state.current_mp ||
            stored.level != state.level ||
            stored.total_exp != state.total_exp) {
            stored = state;
            changed = true;
        }
    }

    if (changed) {
        ++runtime_stats.revision_;
    }
    result.runtime_state_changed = changed;
    return result;
}

} // namespace game::domain
