#pragma once

#include "game/runtime/localization_service.h"

#include <string>
#include <string_view>

namespace game::ui {

/// @brief Resolve a catalog text key at the presentation boundary.
[[nodiscard]] inline std::string localizeKeyOrFallback(const game::runtime::LocalizationService* localization,
                                                       std::string_view key,
                                                       std::string_view fallback_id = {}) {
    if (key.empty()) {
        return std::string{fallback_id};
    }
    if (!localization) {
        return std::string{key};
    }

    const bool has_text = localization->hasText(key);
    const std::string text = localization->tr(key);
    if (!has_text && !fallback_id.empty()) {
        return std::string{fallback_id};
    }
    return text;
}

} // namespace game::ui
