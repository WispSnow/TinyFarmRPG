#include "game/battle/battle_formula_evaluator.h"

#include <sol/sol.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace game::battle {

namespace {

const std::string kChunkName = "battle_formula_evaluator";

void installUnitTable(sol::state& lua, const char* key, const BattleUnit& unit) {
    lua[key] = lua.create_table_with(
        "hp", unit.hp,
        "mp", unit.mp,
        "mhp", unit.max_hp,
        "mmp", unit.max_mp,
        "atk", unit.attack,
        "def", unit.defense,
        "mat", unit.magic_attack,
        "mdf", unit.magic_defense,
        "agi", unit.speed,
        "luk", unit.luck);
}

bool readNumericResult(const sol::object& value, int& out_value) {
    double number = 0.0;
    if (value.is<int>()) {
        number = static_cast<double>(value.as<int>());
    } else if (value.is<double>()) {
        number = value.as<double>();
    } else if (value.is<float>()) {
        number = static_cast<double>(value.as<float>());
    } else {
        return false;
    }

    if (!std::isfinite(number)) {
        return false;
    }

    constexpr double kIntMin = static_cast<double>(std::numeric_limits<int>::min());
    constexpr double kIntMax = static_cast<double>(std::numeric_limits<int>::max());
    const double clamped = std::clamp(number, kIntMin, kIntMax);
    out_value = static_cast<int>(clamped);
    return true;
}

} // namespace

BattleFormulaEvaluator::BattleFormulaEvaluator() {
    lua_.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string, sol::lib::table);
}

bool BattleFormulaEvaluator::evaluate(const std::string_view formula,
                                      const BattleUnit& source,
                                      const BattleUnit& target,
                                      int& out_value,
                                      std::string& out_error) {
    out_value = 0;

    if (formula.empty()) {
        out_error = "battle formula is empty";
        return false;
    }

    installUnitTable(lua_, "a", source);
    installUnitTable(lua_, "b", target);

    const std::string script = "return (" + std::string(formula) + ")";
    const sol::load_result chunk = lua_.load(script, kChunkName);
    if (!chunk.valid()) {
        const sol::error err = chunk;
        out_error = err.what();
        return false;
    }

    sol::protected_function fn = chunk;
    sol::protected_function_result result = fn();
    if (!result.valid()) {
        const sol::error err = result;
        out_error = err.what();
        return false;
    }

    if (result.return_count() < 1) {
        out_error = "battle formula did not return a value";
        return false;
    }

    const sol::object value = result.get<sol::object>();
    if (!readNumericResult(value, out_value)) {
        out_error = "battle formula result is not a finite number";
        return false;
    }

    out_error.clear();
    return true;
}

bool BattleFormulaEvaluator::evaluate(const game::data::DamageFormulaData& damage_formula,
                                      const BattleUnit& source,
                                      const BattleUnit& target,
                                      int& out_value,
                                      std::string& out_error) {
    return evaluate(damage_formula.formula, source, target, out_value, out_error);
}

} // namespace game::battle
