#include <gtest/gtest.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

namespace {

struct SourceViolation {
    std::filesystem::path path{};
    int line{1};
    std::string text{};
};

[[nodiscard]] std::filesystem::path projectRoot() {
    return std::filesystem::path{PROJECT_SOURCE_DIR};
}

[[nodiscard]] std::string slurp(const std::filesystem::path& path) {
    std::ifstream file(path);
    EXPECT_TRUE(file.is_open()) << "Failed to open " << path;
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

[[nodiscard]] int lineForOffset(const std::string& source, const std::size_t offset) {
    int line = 1;
    const std::size_t limit = std::min(offset, source.size());
    for (std::size_t index = 0; index < limit; ++index) {
        if (source[index] == '\n') {
            ++line;
        }
    }
    return line;
}

[[nodiscard]] bool hasAsciiLetter(const std::string_view text) {
    for (const char ch : text) {
        if (std::isalpha(static_cast<unsigned char>(ch)) != 0) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool blockUsesLocalizationHelper(const std::string_view block) {
    static constexpr std::string_view kHelpers[] = {
        "localizeTextOrFallback(",
        "formatTextOrFallback(",
        "tryLocalize(",
        "localizeOptionalKeyOrFallback(",
        "localization_->tr(",
        "localization_->format(",
        "localization->tr(",
        "localization->format(",
        "localization.tr(",
        "localization.format(",
    };
    for (const std::string_view helper : kHelpers) {
        if (block.find(helper) != std::string_view::npos) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool hasOnlyTechnicalLetters(const std::string_view text) {
    static constexpr std::string_view kAllowed[] = {
        "none",
        "TinyFarm",
    };
    for (const std::string_view allowed : kAllowed) {
        if (text == allowed) {
            return true;
        }
    }
    if (text.rfind("image(", 0) == 0) {
        return true;
    }
    if (text.size() >= 2 && text[0] == 'x' &&
        std::all_of(text.begin() + 1, text.end(), [](const char ch) {
            return std::isdigit(static_cast<unsigned char>(ch)) != 0 || ch == '.';
        })) {
        return true;
    }
    if (text.size() > 2 && text.substr(text.size() - 2) == "dp") {
        return std::all_of(text.begin(), text.end() - 2, [](const char ch) {
            return std::isdigit(static_cast<unsigned char>(ch)) != 0 || ch == '.' || ch == '-';
        });
    }
    return false;
}

[[nodiscard]] bool isLocalizedPlaceholder(const std::string_view text) {
    return text.size() >= 2 && text.front() == '!' && text.back() == '!';
}

[[nodiscard]] std::vector<SourceViolation> collectRmlStringDefaultLiteralViolations() {
    const std::regex rml_string_regex{R"re(Rml::String\s+\w+\s*\{\s*"([^"\\]*(?:\\.[^"\\]*)*)")re"};
    std::vector<SourceViolation> violations{};
    const std::filesystem::path src_dir = projectRoot() / "src" / "game";

    for (const auto& entry : std::filesystem::recursive_directory_iterator(src_dir)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        if (entry.path().extension() != ".h" && entry.path().extension() != ".cpp") {
            continue;
        }
        if (entry.path().string().find("/debug/") != std::string::npos) {
            continue;
        }

        const std::string source = slurp(entry.path());
        for (std::sregex_iterator it(source.begin(), source.end(), rml_string_regex), last; it != last; ++it) {
            const std::string literal = (*it)[1].str();
            if (!hasAsciiLetter(literal) || isLocalizedPlaceholder(literal) || hasOnlyTechnicalLetters(literal)) {
                continue;
            }
            violations.push_back(SourceViolation{
                .path = entry.path(),
                .line = lineForOffset(source, static_cast<std::size_t>(it->position())),
                .text = literal,
            });
        }
    }

    return violations;
}

[[nodiscard]] std::vector<SourceViolation> collectTimedNotificationLiteralViolations() {
    const std::regex string_literal_regex{R"re("([^"\\]*(?:\\.[^"\\]*)*)")re"};
    std::vector<SourceViolation> violations{};
    const std::filesystem::path src_dir = projectRoot() / "src" / "game";

    for (const auto& entry : std::filesystem::recursive_directory_iterator(src_dir)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".cpp") {
            continue;
        }
        if (entry.path().string().find("/debug/") != std::string::npos) {
            continue;
        }

        const std::string source = slurp(entry.path());
        std::size_t pos = 0;
        while ((pos = source.find("showTimedNotification(", pos)) != std::string::npos) {
            const std::size_t end = source.find(");", pos);
            if (end == std::string::npos) {
                break;
            }

            const std::string block = source.substr(pos, end - pos + 2);
            if (!blockUsesLocalizationHelper(block)) {
                for (std::sregex_iterator it(block.begin(), block.end(), string_literal_regex), last; it != last; ++it) {
                    const std::string literal = (*it)[1].str();
                    if (!literal.empty() && hasAsciiLetter(literal)) {
                        violations.push_back(SourceViolation{
                            .path = entry.path(),
                            .line = lineForOffset(source, pos + static_cast<std::size_t>(it->position())),
                            .text = literal,
                        });
                    }
                }
            }
            pos = end + 2;
        }
    }

    return violations;
}

[[nodiscard]] std::vector<SourceViolation> collectLuaDialogueLiteralViolations() {
    const std::regex direct_dialogue_regex{
        R"((?:tf\.)?dialogue\.(?:show|choice)\s*\(\s*["']([^"']*[A-Za-z][^"']*)["'])"};
    std::vector<SourceViolation> violations{};
    const std::filesystem::path scripts_dir = projectRoot() / "scripts";

    for (const auto& entry : std::filesystem::recursive_directory_iterator(scripts_dir)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".lua") {
            continue;
        }

        const std::string source = slurp(entry.path());
        for (std::sregex_iterator it(source.begin(), source.end(), direct_dialogue_regex), last; it != last; ++it) {
            violations.push_back(SourceViolation{
                .path = entry.path(),
                .line = lineForOffset(source, static_cast<std::size_t>(it->position())),
                .text = (*it)[1].str(),
            });
        }
    }

    return violations;
}

} // namespace

TEST(LocalizationSourceGuardTest, TimedNotificationsDoNotUseRawEnglishLiterals) {
    const std::vector<SourceViolation> violations = collectTimedNotificationLiteralViolations();
    for (const auto& violation : violations) {
        ADD_FAILURE() << violation.path << ':' << violation.line
                      << " passes raw text to showTimedNotification: " << violation.text;
    }
}

TEST(LocalizationSourceGuardTest, ProjectLuaDialogueCallsDoNotUseRawEnglishLiterals) {
    const std::vector<SourceViolation> violations = collectLuaDialogueLiteralViolations();
    for (const auto& violation : violations) {
        ADD_FAILURE() << violation.path << ':' << violation.line
                      << " passes raw text to dialogue API: " << violation.text;
    }
}

TEST(LocalizationSourceGuardTest, BoundRmlStringDefaultsDoNotUseRawEnglishLiterals) {
    const std::vector<SourceViolation> violations = collectRmlStringDefaultLiteralViolations();
    for (const auto& violation : violations) {
        ADD_FAILURE() << violation.path << ':' << violation.line
                      << " initializes a bound Rml::String with raw English: " << violation.text;
    }
}

TEST(LocalizationSourceGuardTest, TitleLoadFailureMessageUsesLocalizedKey) {
    const std::filesystem::path source_path = projectRoot() / "src" / "game" / "scene" / "game_scene.cpp";
    const std::string source = slurp(source_path);
    EXPECT_EQ(source.find("Load failed: "), std::string::npos);
    EXPECT_NE(source.find("title.message.load_failed_detail"), std::string::npos);
}
