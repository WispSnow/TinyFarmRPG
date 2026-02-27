#pragma once

#include "game/battle/battle_types.h"

#include <string>
#include <vector>

namespace game::data {
class RpgCatalog;
}

namespace game::battle {

// Keep factory input independent from defs::EnterBattleCommand to avoid
// coupling battle/data assembly helpers to command/event definitions.
struct BattleUnitBuildOptions {
    std::vector<std::string> actor_ids{};
    std::string troop_id{};
};

[[nodiscard]] bool buildBattleUnitsFromCatalog(const game::data::RpgCatalog& catalog,
                                               const BattleUnitBuildOptions& options,
                                               std::vector<BattleUnit>& out_units,
                                               std::string& out_error);

} // namespace game::battle
