#pragma once

#include "game/battle/battle_formula_evaluator.h"
#include "game/battle/battle_runtime_types.h"
#include "game/battle/battle_types.h"

#include <functional>
#include <random>

namespace game::battle {

class TurnCore;

class BattleActionResolver final {
public:
    using EscapeRollFunc = std::function<int()>;

private:
    BattleFormulaEvaluator formula_evaluator_{};
    std::mt19937 random_engine_{std::random_device{}()};
    EscapeRollFunc escape_roll_override_{};

public:
    BattleActionResolver() = default;
    explicit BattleActionResolver(EscapeRollFunc escape_roll_override);

    [[nodiscard]] BattleActionResult resolve(const BattleAction& action,
                                             TurnCore& turn_core,
                                             BattleRuntimeState& runtime_state);

private:
    [[nodiscard]] int nextEscapeRoll();
};

} // namespace game::battle
