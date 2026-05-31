#include "game/runtime/localization_service.h"

#include <gtest/gtest.h>
#include <spdlog/sinks/ostream_sink.h>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

namespace {

void writeFile(const std::filesystem::path& path, const std::string& content) {
    std::error_code ec{};
    std::filesystem::create_directories(path.parent_path(), ec);
    std::ofstream file(path);
    file << content;
}

[[nodiscard]] std::filesystem::path testDir() {
    std::filesystem::path dir = std::filesystem::temp_directory_path() / "tinyfarm_localization_service_test";
    std::error_code ec{};
    std::filesystem::remove_all(dir, ec);
    std::filesystem::create_directories(dir, ec);
    return dir;
}

[[nodiscard]] std::string manifestPath() {
    return (std::filesystem::path{PROJECT_SOURCE_DIR} / "assets/i18n/languages.json").lexically_normal().string();
}

class ScopedDefaultLogger final {
public:
    explicit ScopedDefaultLogger(std::shared_ptr<spdlog::logger> logger)
        : previous_(spdlog::default_logger()) {
        spdlog::set_default_logger(std::move(logger));
    }

    ~ScopedDefaultLogger() {
        spdlog::set_default_logger(std::move(previous_));
    }

    ScopedDefaultLogger(const ScopedDefaultLogger&) = delete;
    ScopedDefaultLogger& operator=(const ScopedDefaultLogger&) = delete;

private:
    std::shared_ptr<spdlog::logger> previous_{};
};

} // namespace

TEST(LocalizationServiceTest, LoadsManifestAndTranslatesCurrentLanguage) {
    game::runtime::LocalizationService localization;
    ASSERT_TRUE(localization.loadLanguageIndex(manifestPath()));

    EXPECT_EQ(localization.fallbackLanguageTag(), "en-US");
    ASSERT_TRUE(localization.setLanguage("zh-Hans"));
    EXPECT_EQ(localization.currentLanguageTag(), "zh-Hans");
    EXPECT_EQ(localization.tr("options.language"), "语言");
    EXPECT_EQ(localization.languageNativeName("zh-Hans"), "简体中文");
}

TEST(LocalizationServiceTest, MissingCurrentLanguageKeyFallsBackToManifestFallback) {
    const std::filesystem::path dir = testDir();
    writeFile(dir / "languages.json",
              R"({
                "fallback": "en-US",
                "languages": [
                  {"tag": "en-US", "native_name": "English", "file": ")" + (dir / "en-US.json").string() + R"("},
                  {"tag": "zh-Hans", "native_name": "简体中文", "file": ")" + (dir / "zh-Hans.json").string() + R"("}
                ]
              })");
    writeFile(dir / "en-US.json", R"({"only.en": "Only English", "hello": "Hello {name}"})");
    writeFile(dir / "zh-Hans.json", R"({"hello": "你好 {name}"})");

    game::runtime::LocalizationService localization;
    ASSERT_TRUE(localization.loadLanguageIndex((dir / "languages.json").string()));
    ASSERT_TRUE(localization.setLanguage("zh-Hans"));

    EXPECT_EQ(localization.tr("only.en"), "Only English");
    EXPECT_EQ(localization.tr("missing.key"), "!missing.key!");
    EXPECT_EQ(localization.format("hello", std::unordered_map<std::string, std::string>{{"name", "Ada"}}), "你好 Ada");
}

TEST(LocalizationServiceTest, HasTextChecksCurrentAndFallbackTables) {
    const std::filesystem::path dir = testDir();
    writeFile(dir / "languages.json",
              R"({
                "fallback": "en-US",
                "languages": [
                  {"tag": "en-US", "native_name": "English", "file": ")" + (dir / "en-US.json").string() + R"("},
                  {"tag": "zh-Hans", "native_name": "简体中文", "file": ")" + (dir / "zh-Hans.json").string() + R"("}
                ]
              })");
    writeFile(dir / "en-US.json", R"({"fallback.only": "Fallback", "both": "Both"})");
    writeFile(dir / "zh-Hans.json", R"({"both": "双方"})");

    game::runtime::LocalizationService localization;
    ASSERT_TRUE(localization.loadLanguageIndex((dir / "languages.json").string()));
    ASSERT_TRUE(localization.setLanguage("zh-Hans"));

    EXPECT_TRUE(localization.hasText("both"));
    EXPECT_TRUE(localization.hasText("fallback.only"));
    EXPECT_FALSE(localization.hasText("missing.key"));
}

TEST(LocalizationServiceTest, UnknownLanguageResolvesToConfiguredFallback) {
    game::runtime::LocalizationService localization;
    ASSERT_TRUE(localization.loadLanguageIndex(manifestPath()));

    EXPECT_EQ(localization.resolveSupportedLanguageTag("fr-FR"), "en-US");
    EXPECT_FALSE(localization.isSupportedLanguage("fr-FR"));
}

TEST(LocalizationServiceTest, TargetLanguageFileFailureFallsBackAndStillSucceeds) {
    const std::filesystem::path dir = testDir();
    writeFile(dir / "languages.json",
              R"({
                "fallback": "en-US",
                "languages": [
                  {"tag": "en-US", "native_name": "English", "file": ")" + (dir / "en-US.json").string() + R"("},
                  {"tag": "zh-Hans", "native_name": "简体中文", "file": ")" + (dir / "missing.json").string() + R"("}
                ]
              })");
    writeFile(dir / "en-US.json", R"({"hello": "Hello"})");

    game::runtime::LocalizationService localization;
    ASSERT_TRUE(localization.loadLanguageIndex((dir / "languages.json").string()));

    EXPECT_TRUE(localization.setLanguage("zh-Hans"));
    EXPECT_EQ(localization.currentLanguageTag(), "en-US");
    EXPECT_EQ(localization.tr("hello"), "Hello");
}

TEST(LocalizationServiceTest, FallbackLanguageFileFailureRejectsManifestAndKeepsPreviousState) {
    const std::filesystem::path dir = testDir();
    const std::filesystem::path good_dir = dir / "good";
    const std::filesystem::path bad_dir = dir / "bad";
    writeFile(good_dir / "languages.json",
              R"({
                "fallback": "en-US",
                "languages": [
                  {"tag": "en-US", "native_name": "English", "file": ")" + (good_dir / "en-US.json").string() + R"("}
                ]
              })");
    writeFile(good_dir / "en-US.json", R"({"hello": "Hello"})");
    writeFile(bad_dir / "languages.json",
              R"({
                "fallback": "en-US",
                "languages": [
                  {"tag": "en-US", "native_name": "English", "file": ")" + (bad_dir / "missing.json").string() + R"("}
                ]
              })");

    game::runtime::LocalizationService localization;
    ASSERT_TRUE(localization.loadLanguageIndex((good_dir / "languages.json").string()));
    ASSERT_EQ(localization.tr("hello"), "Hello");

    EXPECT_FALSE(localization.loadLanguageIndex((bad_dir / "languages.json").string()));
    EXPECT_EQ(localization.currentLanguageTag(), "en-US");
    EXPECT_EQ(localization.tr("hello"), "Hello");
}

TEST(LocalizationServiceTest, FormatWarnsOnceForUnresolvedPlaceholders) {
    const std::filesystem::path dir = testDir();
    writeFile(dir / "languages.json",
              R"({
                "fallback": "en-US",
                "languages": [
                  {"tag": "en-US", "native_name": "English", "file": ")" + (dir / "en-US.json").string() + R"("}
                ]
              })");
    writeFile(dir / "en-US.json", R"({"reward": "Gained {amount} from {source}"})");

    std::ostringstream logs;
    auto sink = std::make_shared<spdlog::sinks::ostream_sink_mt>(logs);
    auto logger = std::make_shared<spdlog::logger>("localization_service_test", sink);
    logger->set_level(spdlog::level::warn);
    ScopedDefaultLogger scoped_logger{logger};

    game::runtime::LocalizationService localization;
    ASSERT_TRUE(localization.loadLanguageIndex((dir / "languages.json").string()));

    EXPECT_EQ(localization.format("reward", {{"amount", "50"}}), "Gained 50 from {source}");
    EXPECT_EQ(localization.format("reward", {{"amount", "75"}}), "Gained 75 from {source}");

    const std::string output = logs.str();
    EXPECT_NE(output.find("reward"), std::string::npos);
    EXPECT_EQ(output.find("reward"), output.rfind("reward"));
}
