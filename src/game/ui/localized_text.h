#pragma once

#include "game/runtime/localization_service.h"
#include "game/ui/text_utils.h"

#include <string>
#include <string_view>
#include <unordered_map>

namespace game::ui {

using LocalizedFormatArgs = std::unordered_map<std::string, std::string>;

/// @brief Resolve a text key when a data source is being migrated from raw text to keys.
[[nodiscard]] inline std::string tryLocalize(const game::runtime::LocalizationService* localization,
                                             std::string_view key_or_text) {
    if (localization && !key_or_text.empty() && localization->hasText(key_or_text)) {
        return localization->tr(key_or_text);
    }
    return std::string{key_or_text};
}

/// @brief Resolve a catalog id display name stored as `<id>.name`, falling back to a readable id label.
[[nodiscard]] inline std::string localizeIdName(const game::runtime::LocalizationService* localization,
                                                std::string_view id,
                                                const char* fallback = "") {
    const std::string key = std::string{id} + ".name";
    if (localization && localization->hasText(key)) {
        return localization->tr(key);
    }
    return humanizeId(id, fallback);
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
