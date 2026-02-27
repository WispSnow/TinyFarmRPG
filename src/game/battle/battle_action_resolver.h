#pragma once

#include "game/battle/battle_formula_evaluator.h"
#include "game/battle/battle_runtime_types.h"
#include "game/battle/battle_types.h"

#include <cstdint>
#include <functional>
#include <random>
#include <string>
#include <vector>

namespace game::data {
class RpgCatalog;
class ItemCatalog;
struct SkillData;
enum class Scope : std::uint8_t;
} // namespace game::data

namespace game::battle {

class TurnCore;

class BattleActionResolver final {
public:
    using EscapeRollFunc = std::function<int()>;
    struct Dependencies {
        const game::data::RpgCatalog* rpg_catalog{nullptr};
        const game::data::ItemCatalog* item_catalog{nullptr};
    };

private:
    Dependencies dependencies_{};
    BattleFormulaEvaluator formula_evaluator_{};
    std::mt19937 random_engine_{std::random_device{}()};
    EscapeRollFunc escape_roll_override_{};

public:
    BattleActionResolver() = default;
    explicit BattleActionResolver(EscapeRollFunc escape_roll_override);
    explicit BattleActionResolver(Dependencies dependencies, EscapeRollFunc escape_roll_override = {});

    [[nodiscard]] BattleActionResult resolve(const BattleAction& action,
                                             TurnCore& turn_core,
                                             BattleRuntimeState& runtime_state);

private:
    [[nodiscard]] int nextPercentRoll();
    [[nodiscard]] bool collectSkillTargets(const BattleAction& action,
                                           const BattleUnit& actor,
                                           const game::data::SkillData& skill,
                                           TurnCore& turn_core,
                                           std::vector<BattleUnit*>& out_targets,
                                           std::string& out_error);
    void applySkillEffects(const game::data::SkillData& skill,
                           BattleUnit& target,
                           BattleRuntimeState::UnitRuntimeState& target_state,
                           BattleActionResult& result);
    [[nodiscard]] int nextEscapeRoll();
};

} // namespace game::battle
