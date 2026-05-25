#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace game::runtime {

struct LocalizationLanguageInfo {
    std::string tag{};
    std::string native_name{};
    std::string file{};
};

/// @brief Runtime text localization service.
///
/// Language files are flat JSON objects where keys are stable text ids and values are localized strings.
/// Missing keys resolve through the configured fallback language before returning `!key!`.
class LocalizationService final {
public:
    [[nodiscard]] bool loadLanguageIndex(std::string_view manifest_path);
    [[nodiscard]] bool setLanguage(std::string_view language_tag);

    [[nodiscard]] std::string tr(std::string_view key) const;
    [[nodiscard]] std::string format(
        std::string_view key,
        const std::unordered_map<std::string, std::string>& args) const;
    [[nodiscard]] bool hasText(std::string_view key) const;

    [[nodiscard]] std::string resolveSupportedLanguageTag(std::string_view language_tag) const;
    [[nodiscard]] bool isSupportedLanguage(std::string_view language_tag) const;
    [[nodiscard]] std::string languageNativeName(std::string_view language_tag) const;

    [[nodiscard]] const std::vector<LocalizationLanguageInfo>& languages() const noexcept { return languages_; }
    [[nodiscard]] std::string_view currentLanguageTag() const noexcept { return current_language_tag_; }
    [[nodiscard]] std::string_view fallbackLanguageTag() const noexcept { return fallback_language_tag_; }

private:
    [[nodiscard]] const LocalizationLanguageInfo* findLanguage(std::string_view language_tag) const;
    [[nodiscard]] bool loadLanguageFile(const LocalizationLanguageInfo& info);
    [[nodiscard]] const std::unordered_map<std::string, std::string>* tableFor(std::string_view language_tag) const;
    [[nodiscard]] std::string missingText(std::string_view key) const;
    void warnMissingOnce(std::string_view key) const;
    void warnUnresolvedPlaceholdersOnce(std::string_view key, std::string_view text) const;

    std::vector<LocalizationLanguageInfo> languages_{};
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> tables_{};
    std::string current_language_tag_{"en-US"};
    std::string fallback_language_tag_{"en-US"};
    mutable std::unordered_set<std::string> missing_key_warnings_{};
    mutable std::unordered_set<std::string> unresolved_placeholder_warnings_{};
};

} // namespace game::runtime
