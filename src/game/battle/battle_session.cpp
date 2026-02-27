#include "battle_session.h"

namespace game::battle {

BattleSession::BattleSession(std::vector<BattleUnit> units)
    : turn_core_(std::move(units)) {
    for (const auto& unit : turn_core_.units()) {
        runtime_state_.units.try_emplace(unit.id);
    }
    turn_core_.setRoundHooks(
        [this](std::uint32_t) {
            for (auto& [unit_id, state] : runtime_state_.units) {
                (void)unit_id;
                state.guarding = false;
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
