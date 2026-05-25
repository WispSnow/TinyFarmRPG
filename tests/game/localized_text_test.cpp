#include "game/ui/localized_text.h"

#include <gtest/gtest.h>

#include <filesystem>

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

namespace {

[[nodiscard]] std::string manifestPath() {
    return (std::filesystem::path{PROJECT_SOURCE_DIR} / "assets/i18n/languages.json").lexically_normal().string();
}

} // namespace

TEST(LocalizedTextTest, ResolvesCatalogKeyThroughLocalizationService) {
    game::runtime::LocalizationService localization;
    ASSERT_TRUE(localization.loadLanguageIndex(manifestPath()));
    ASSERT_TRUE(localization.setLanguage("zh-Hans"));

    EXPECT_EQ(game::ui::localizeKeyOrFallback(&localization, "options.language", "options"), "语言");
}

TEST(LocalizedTextTest, MissingKeyCanFallBackToStableId) {
    game::runtime::LocalizationService localization;
    ASSERT_TRUE(localization.loadLanguageIndex(manifestPath()));

    EXPECT_EQ(game::ui::localizeKeyOrFallback(&localization, "missing.key", "stable_id"), "stable_id");
}

TEST(LocalizedTextTest, NullLocalizationKeepsRawKey) {
    EXPECT_EQ(game::ui::localizeKeyOrFallback(nullptr, "item.tool_hoe.name", "tool_hoe"), "item.tool_hoe.name");
}
