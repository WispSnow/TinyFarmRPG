#include "battle_session.h"

#include <entt/entity/entity.hpp>

namespace game::battle {

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
}

BattleSnapshot BattleSession::snapshot() const {
    BattleSnapshot snapshot{};
    snapshot.units = turn_core_.units();
    snapshot.current_actor_id = turn_core_.currentActorId();
    snapshot.round_index = turn_core_.roundIndex();
    snapshot.outcome = turn_core_.outcome();
    return snapshot;
}

BattleActionResult BattleSession::submitAction(const BattleAction& action) {
    BattleActionResult result = resolver_.resolve(action, turn_core_, runtime_state_);

    fillSnapshot(result);
    return result;
}

void BattleSession::fillSnapshot(BattleActionResult& result) const {
    result.snapshot = snapshot();
    result.outcome_after = result.snapshot.outcome;
}

} // namespace game::battle
