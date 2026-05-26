#pragma once

#include "game/battle/battle_types.h"
#include "game/runtime/localization_service.h"
#include "game/ui/localized_text.h"

#include <string>
#include <string_view>

namespace game::battle {

[[nodiscard]] inline std::string localizedUnitName(const BattleUnit& unit,
                                                   const game::runtime::LocalizationService* localization,
                                                   const std::string_view fallback_prefix = "Unit") {
    if (unit.name.empty()) {
        return std::string{fallback_prefix} + " #" + std::to_string(unit.id);
    }

    std::string label = game::ui::tryLocalize(localization, unit.name);
    if (unit.name_ordinal > 1) {
        label += " " + std::to_string(unit.name_ordinal);
    }
    return label;
}

} // namespace game::battle
