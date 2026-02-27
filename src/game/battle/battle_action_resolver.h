#pragma once

#include "game/battle/battle_formula_evaluator.h"
#include "game/battle/battle_types.h"

namespace game::battle {

class TurnCore;

class BattleActionResolver final {
    BattleFormulaEvaluator formula_evaluator_{};

public:
    [[nodiscard]] BattleActionResult resolve(const BattleAction& action, TurnCore& turn_core);
};

} // namespace game::battle
