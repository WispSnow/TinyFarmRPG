#pragma once

#include "battle_types.h"
#include "turn_core.h"

#include <optional>
#include <vector>

namespace game::battle {

class BattleSession final {
    TurnCore turn_core_;

public:
    explicit BattleSession(std::vector<BattleUnit> units);

    [[nodiscard]] const std::vector<BattleUnit>& units() const { return turn_core_.units(); }
    [[nodiscard]] const BattleUnit* findUnit(BattleUnitId id) const { return turn_core_.findUnit(id); }
    [[nodiscard]] std::optional<BattleUnitId> currentActorId() const { return turn_core_.currentActorId(); }
    [[nodiscard]] BattleOutcome outcome() const { return turn_core_.outcome(); }
    [[nodiscard]] BattleSnapshot snapshot() const;

    [[nodiscard]] BattleActionResult submitAction(const BattleAction& action);

private:
    [[nodiscard]] BattleActionResult makeRejectedResult(const BattleAction& action) const;
    void fillSnapshot(BattleActionResult& result) const;
};

} // namespace game::battle
