#pragma once

#include "game/battle/battle_types.h"
#include "game/data/rpg_data.h"

#ifdef TF_ENABLE_SCRIPTING
#include <sol/sol.hpp>
#endif

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

#ifdef TF_ENABLE_SCRIPTING
private:
    sol::state lua_{};
#endif
};

} // namespace game::battle
