#pragma once

#include "game/runtime/localization_service.h"

#include <string>
#include <string_view>
#include <unordered_map>

namespace game::ui {

using LocalizedFormatArgs = std::unordered_map<std::string, std::string>;

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

/// @brief Resolve a required UI text key. Missing keys intentionally surface as `!key!`.
[[nodiscard]] inline std::string localizeRequiredText(const game::runtime::LocalizationService* localization,
                                                      std::string_view key) {
    return localization ? localization->tr(key) : std::string{"!"} + std::string{key} + "!";
}

/// @brief Resolve a required formatted UI text key. Missing keys intentionally surface as `!key!`.
[[nodiscard]] inline std::string formatRequiredText(const game::runtime::LocalizationService* localization,
                                                    std::string_view key,
                                                    const LocalizedFormatArgs& args) {
    return localization ? localization->format(key, args) : std::string{"!"} + std::string{key} + "!";
}

/// @brief Resolve optional UI text for legacy paths that still lack a LocalizationService pointer.
[[nodiscard]] inline std::string localizeTextOrFallback(const game::runtime::LocalizationService* localization,
                                                       std::string_view key,
                                                       std::string_view fallback) {
    return localization ? localization->tr(key) : std::string{fallback};
}

/// @brief Resolve optional formatted UI text for legacy paths that still lack a LocalizationService pointer.
template<typename FallbackFn>
[[nodiscard]] std::string formatTextOrFallback(const game::runtime::LocalizationService* localization,
                                               std::string_view key,
                                               const LocalizedFormatArgs& args,
                                               FallbackFn&& fallback_fn) {
    return localization ? localization->format(key, args) : fallback_fn();
}

} // namespace game::ui
