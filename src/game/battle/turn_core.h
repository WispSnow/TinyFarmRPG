#pragma once

#include "battle_types.h"

#include <cstddef>
#include <optional>
#include <unordered_map>
#include <vector>

namespace game::battle {

class TurnCore final {
    std::vector<BattleUnit> units_{};
    std::unordered_map<BattleUnitId, std::size_t> unit_index_by_id_{};
    std::vector<BattleUnitId> turn_order_{};
    std::size_t current_turn_index_{0};
    BattleOutcome outcome_{BattleOutcome::Ongoing};

public:
    explicit TurnCore(std::vector<BattleUnit> units);

    [[nodiscard]] const std::vector<BattleUnit>& units() const { return units_; }
    [[nodiscard]] const std::vector<BattleUnitId>& turnOrder() const { return turn_order_; }

    [[nodiscard]] const BattleUnit* findUnit(BattleUnitId id) const;
    [[nodiscard]] BattleUnit* findUnitMutable(BattleUnitId id);

    [[nodiscard]] std::optional<BattleUnitId> currentActorId() const;
    [[nodiscard]] BattleOutcome outcome() const { return outcome_; }
    [[nodiscard]] bool isBattleEnded() const { return outcome_ != BattleOutcome::Ongoing; }

    [[nodiscard]] bool advanceTurn();
    void refresh();

private:
    void buildIndex();
    void buildTurnOrder();
    void evaluateOutcome();
    void alignCurrentActor();
    [[nodiscard]] bool isAlive(BattleUnitId id) const;
};

} // namespace game::battle
