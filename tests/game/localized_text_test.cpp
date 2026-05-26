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

TEST(LocalizedTextTest, TryLocalizeResolvesKnownKeyThroughLocalizationService) {
    game::runtime::LocalizationService localization;
    ASSERT_TRUE(localization.loadLanguageIndex(manifestPath()));
    ASSERT_TRUE(localization.setLanguage("zh-Hans"));

    EXPECT_EQ(game::ui::tryLocalize(&localization, "options.language"), "语言");
}

TEST(LocalizedTextTest, TryLocalizeKeepsUnknownText) {
    game::runtime::LocalizationService localization;
    ASSERT_TRUE(localization.loadLanguageIndex(manifestPath()));

    EXPECT_EQ(game::ui::tryLocalize(&localization, "Raw catalog text"), "Raw catalog text");
}

TEST(LocalizedTextTest, TryLocalizeKeepsRawTextWithoutLocalization) {
    EXPECT_EQ(game::ui::tryLocalize(nullptr, "item.tool_hoe.name"), "item.tool_hoe.name");
}
