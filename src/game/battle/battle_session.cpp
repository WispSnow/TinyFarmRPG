#include "battle_session.h"

#include "game/data/rpg_catalog.h"

#include <entt/entity/entity.hpp>

#include <algorithm>
#include <string>
#include <vector>

namespace game::battle {
namespace {

[[nodiscard]] int statePriority(const game::data::RpgCatalog* rpg_catalog, const std::string& state_id) {
    if (!rpg_catalog) {
        return 0;
    }

    const auto* state = rpg_catalog->findState(state_id);
    return state ? state->priority_ : 0;
}

} // namespace

BattleSession::BattleSession(std::vector<BattleUnit> units, BattleSessionOptions options)
    : turn_core_(std::move(units)),
      resolver_(BattleActionResolver::Dependencies{
          .rpg_catalog = options.rpg_catalog,
          .item_catalog = options.item_catalog}) {
    for (const auto& [item_id, count] : options.item_stocks) {
        if (item_id != entt::null && count > 0) {
            runtime_state_.item_stocks.insert_or_assign(item_id, count);
        }
    }

    for (const auto& unit : turn_core_.units()) {
        runtime_state_.units.try_emplace(unit.id);
    }
    turn_core_.setRoundHooks(
        [this](std::uint32_t) {
            for (auto& [unit_id, state] : runtime_state_.units) {
                (void)unit_id;
                state.guarding = false;
                for (auto it = state.state_turns_left.begin(); it != state.state_turns_left.end();) {
                    --it->second;
                    if (it->second <= 0) {
                        it = state.state_turns_left.erase(it);
                        continue;
                    }
                    ++it;
                }
            }
        },
        {});
    rebuildActiveUnitStates();
}

BattleSnapshot BattleSession::snapshot() const {
    BattleSnapshot snapshot{};
    snapshot.units = turn_core_.units();
    snapshot.turn_order = turn_core_.turnOrder();
    snapshot.unit_states = active_unit_states_;
    snapshot.current_actor_id = turn_core_.currentActorId();
    snapshot.round_index = turn_core_.roundIndex();
    snapshot.outcome = turn_core_.outcome();
    return snapshot;
}

BattleActionResult BattleSession::submitAction(const BattleAction& action) {
    BattleActionResult result = resolver_.resolve(action, turn_core_, runtime_state_);

    rebuildActiveUnitStates();
    fillSnapshot(result);
    return result;
}

void BattleSession::rebuildActiveUnitStates() {
    active_unit_states_.clear();

    for (const auto& unit : turn_core_.units()) {
        if (!unit.isAlive()) {
            continue;
        }

        const auto runtime_it = runtime_state_.units.find(unit.id);
        if (runtime_it == runtime_state_.units.end()) {
            continue;
        }

        std::vector<BattleStateSnapshot> states;
        states.reserve(runtime_it->second.state_turns_left.size());
        for (const auto& [state_id, turns_left] : runtime_it->second.state_turns_left) {
            if (turns_left <= 0) {
                continue;
            }
            states.push_back(BattleStateSnapshot{
                .state_id = state_id,
                .turns_left = turns_left
            });
        }

        if (states.empty()) {
            continue;
        }

        std::sort(states.begin(), states.end(), [this](const BattleStateSnapshot& lhs, const BattleStateSnapshot& rhs) {
            const int lhs_priority = statePriority(resolver_.rpgCatalog(), lhs.state_id);
            const int rhs_priority = statePriority(resolver_.rpgCatalog(), rhs.state_id);
            if (lhs_priority != rhs_priority) {
                return lhs_priority > rhs_priority;
            }
            return lhs.state_id < rhs.state_id;
        });

        active_unit_states_.push_back(BattleUnitStateSnapshot{
            .unit_id = unit.id,
            .states = std::move(states)
        });
    }
}

void BattleSession::fillSnapshot(BattleActionResult& result) const {
    result.snapshot = snapshot();
    result.outcome_after = result.snapshot.outcome;
}

} // namespace game::battle
