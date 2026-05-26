#include "game/domain/party_rest_service.h"

#include "game/battle/actor_stats_resolver.h"
#include "game/component/party_component.h"
#include "game/component/party_equipment_component.h"
#include "game/data/rpg_catalog.h"
#include "game/data/rpg_data.h"
#include "game/defs/party_ids.h"
#include "game/domain/actor_progression_service.h"

#include <entt/entity/registry.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstddef>
#include <string_view>
#include <utility>
#include <vector>

namespace game::domain {
namespace {

constexpr int kRecoveryPercentPerHour = 10;
constexpr int kFullRecoveryPercent = 100;

[[nodiscard]] bool containsString(const std::vector<std::string>& values, const std::string_view value) {
    return std::any_of(values.begin(), values.end(), [value](const std::string& current) {
        return current == value;
    });
}

[[nodiscard]] std::vector<std::string> activeActorIds(const entt::registry& registry, const entt::entity player) {
    if (player == entt::null || !registry.valid(player)) {
        return {};
    }

    const auto* party = registry.try_get<game::component::PartyComponent>(player);
    if (!party) {
        return {std::string{game::defs::kDefaultPlayerActorId}};
    }
    if (party->active_actor_ids_.empty()) {
        return {};
    }

    std::vector<std::string> actor_ids{};
    actor_ids.reserve(party->active_actor_ids_.size());
    for (const auto& actor_id : party->active_actor_ids_) {
        if (actor_id.empty() || containsString(actor_ids, actor_id)) {
            continue;
        }
        actor_ids.push_back(actor_id);
    }
    return actor_ids;
}

[[nodiscard]] const game::component::ActorEquipmentLoadout* findLoadout(const entt::registry& registry,
                                                                        const entt::entity player,
                                                                        const std::string_view actor_id) {
    const auto* equipment = registry.try_get<game::component::PartyEquipmentComponent>(player);
    if (!equipment) {
        return nullptr;
    }

    const auto it = equipment->loadouts_by_actor_id_.find(std::string{actor_id});
    return it == equipment->loadouts_by_actor_id_.end() ? nullptr : &it->second;
}

[[nodiscard]] const game::component::ActorRuntimeState* findRuntimeState(const entt::registry& registry,
                                                                         const entt::entity player,
                                                                         const std::string_view actor_id) {
    const auto* runtime_stats = registry.try_get<game::component::PartyRuntimeStatsComponent>(player);
    if (!runtime_stats) {
        return nullptr;
    }

    const auto it = runtime_stats->states_by_actor_id_.find(std::string{actor_id});
    return it == runtime_stats->states_by_actor_id_.end() ? nullptr : &it->second;
}

[[nodiscard]] int paramValue(const game::data::ParamArray& params, const game::data::ParamIndex index) {
    return params[static_cast<std::size_t>(index)];
}

[[nodiscard]] int recoveryPercent(const int hours) {
    if (hours <= 0) {
        return 0;
    }
    return std::clamp(hours * kRecoveryPercentPerHour, 0, kFullRecoveryPercent);
}

[[nodiscard]] int recoveryAmount(const int max_value, const int percent) {
    if (max_value <= 0 || percent <= 0) {
        return 0;
    }
    return (max_value * percent + 99) / 100;
}

[[nodiscard]] RestRecoveryMemberPreview buildMemberPreview(const entt::registry& registry,
                                                           const entt::entity player,
                                                           const game::data::RpgCatalog& rpg_catalog,
                                                           const std::string& actor_id,
                                                           const int percent) {
    RestRecoveryMemberPreview member{};
    member.actor_id = actor_id;

    const auto* actor = rpg_catalog.findActor(actor_id);
    if (!actor) {
        spdlog::warn("PartyRestService: actor_id='{}' not found in RpgCatalog; skipping rest recovery preview.",
                     actor_id);
        return {};
    }

    const auto* loadout = findLoadout(registry, player, actor_id);
    const auto* runtime_state = findRuntimeState(registry, player, actor_id);
    const auto state = runtime_state
        ? game::domain::ActorProgressionService::normalizeState(rpg_catalog, *actor, *runtime_state, loadout)
        : game::domain::ActorProgressionService::initialState(rpg_catalog, *actor, loadout);
    const auto resolved = game::battle::resolveActorStats(rpg_catalog, *actor, state.level, loadout);

    member.display_name_key = actor->display_name_.empty() ? actor->id_ : actor->display_name_;
    member.max_hp = std::max(1, paramValue(resolved.params, game::data::ParamIndex::Mhp));
    member.max_mp = std::max(1, paramValue(resolved.params, game::data::ParamIndex::Mmp));
    member.current_hp = std::clamp(state.current_hp, 0, member.max_hp);
    member.current_mp = std::clamp(state.current_mp, 0, member.max_mp);
    member.after_hp = std::clamp(member.current_hp + recoveryAmount(member.max_hp, percent), 0, member.max_hp);
    member.after_mp = std::clamp(member.current_mp + recoveryAmount(member.max_mp, percent), 0, member.max_mp);
    return member;
}

} // namespace

bool RestRecoveryPreview::anyRecovered() const {
    return std::any_of(members.begin(), members.end(), [](const RestRecoveryMemberPreview& member) {
        return member.recovered();
    });
}

RestRecoveryPreview PartyRestService::previewActivePartyRecovery(const entt::registry& registry,
                                                                 const entt::entity player,
                                                                 const game::data::RpgCatalog& rpg_catalog,
                                                                 const int hours) {
    RestRecoveryPreview preview{};
    preview.hours = std::max(0, hours);
    preview.recovery_percent = recoveryPercent(hours);
    preview.full_recovery = preview.recovery_percent >= kFullRecoveryPercent;

    const auto actor_ids = activeActorIds(registry, player);
    preview.members.reserve(actor_ids.size());
    for (const auto& actor_id : actor_ids) {
        auto member = buildMemberPreview(registry, player, rpg_catalog, actor_id, preview.recovery_percent);
        if (member.actor_id.empty()) {
            continue;
        }
        preview.members.push_back(std::move(member));
    }
    return preview;
}

PartyRestApplyResult PartyRestService::applyActivePartyRecovery(entt::registry& registry,
                                                                const entt::entity player,
                                                                const game::data::RpgCatalog& rpg_catalog,
                                                                const int hours) {
    PartyRestApplyResult result{};
    result.preview = previewActivePartyRecovery(registry, player, rpg_catalog, hours);
    if (player == entt::null || !registry.valid(player) || result.preview.empty()) {
        return result;
    }

    auto& runtime_stats = registry.get_or_emplace<game::component::PartyRuntimeStatsComponent>(player);
    bool changed = false;
    for (const auto& member : result.preview.members) {
        const auto* actor = rpg_catalog.findActor(member.actor_id);
        if (!actor) {
            continue;
        }

        const auto* loadout = findLoadout(registry, player, member.actor_id);
        auto state_it = runtime_stats.states_by_actor_id_.find(member.actor_id);
        game::component::ActorRuntimeState state = state_it == runtime_stats.states_by_actor_id_.end()
            ? game::domain::ActorProgressionService::initialState(rpg_catalog, *actor, loadout)
            : game::domain::ActorProgressionService::normalizeState(rpg_catalog, *actor, state_it->second, loadout);

        state.current_hp = member.after_hp;
        state.current_mp = member.after_mp;

        if (state_it == runtime_stats.states_by_actor_id_.end()) {
            if (state.current_hp != member.current_hp || state.current_mp != member.current_mp) {
                runtime_stats.states_by_actor_id_.insert_or_assign(member.actor_id, state);
                changed = true;
            }
            continue;
        }

        if (state_it->second.current_hp != state.current_hp ||
            state_it->second.current_mp != state.current_mp ||
            state_it->second.level != state.level ||
            state_it->second.total_exp != state.total_exp) {
            state_it->second = state;
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
