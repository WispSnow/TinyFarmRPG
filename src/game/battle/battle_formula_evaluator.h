#pragma once

#include "game/battle/battle_types.h"
#include "game/data/rpg_data.h"

#include <sol/sol.hpp>

#include <string>
#include <string_view>

namespace game::battle {

class BattleFormulaEvaluator final {
public:
    BattleFormulaEvaluator();

    [[nodiscard]] bool evaluate(std::string_view formula,
                                const BattleUnit& source,
                                const BattleUnit& target,
                                int& out_value,
                                std::string& out_error);

    [[nodiscard]] bool evaluate(const game::data::DamageFormulaData& damage_formula,
                                const BattleUnit& source,
                                const BattleUnit& target,
                                int& out_value,
                                std::string& out_error);

private:
    sol::state lua_{};
};

} // namespace game::battle
