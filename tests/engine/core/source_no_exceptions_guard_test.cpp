// NOLINTBEGIN
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

namespace {

struct SourceViolation {
    std::filesystem::path path{};
    std::string token{};
};

[[nodiscard]] std::string readTextFile(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return {};
    }
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

[[nodiscard]] bool isScannedSourceFile(const std::filesystem::path& path) {
    const auto extension = path.extension().string();
    return extension == ".cpp" || extension == ".h" || extension == ".hpp" || extension == ".inl";
}

[[nodiscard]] bool isIdentifierChar(const char c) noexcept {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

[[nodiscard]] std::size_t skipWhitespace(std::string_view source, std::size_t pos) noexcept {
    while (pos < source.size()) {
        const char c = source[pos];
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n') {
            break;
        }
        ++pos;
    }
    return pos;
}

[[nodiscard]] bool isStandaloneKeyword(std::string_view source, std::size_t pos, std::string_view keyword) noexcept {
    const bool has_prefix = pos > 0 && isIdentifierChar(source[pos - 1]);
    const std::size_t suffix_pos = pos + keyword.size();
    const bool has_suffix = suffix_pos < source.size() && isIdentifierChar(source[suffix_pos]);
    return !has_prefix && !has_suffix;
}

[[nodiscard]] bool containsForbiddenExceptionKeyword(std::string_view source, std::string_view keyword) noexcept {
    std::size_t pos = source.find(keyword);
    while (pos != std::string_view::npos) {
        if (!isStandaloneKeyword(source, pos, keyword)) {
            pos = source.find(keyword, pos + keyword.size());
            continue;
        }

        if (keyword == "throw") {
            return true;
        }

        const std::size_t next = skipWhitespace(source, pos + keyword.size());
        if (keyword == "try" && next < source.size() && source[next] == '{') {
            return true;
        }
        if (keyword == "catch" && next < source.size() && source[next] == '(') {
            return true;
        }

        pos = source.find(keyword, pos + keyword.size());
    }
    return false;
}

[[nodiscard]] std::vector<SourceViolation> collectExceptionTokenViolations(const std::filesystem::path& root) {
    static constexpr std::string_view kForbiddenKeywords[] = {"try", "catch", "throw"};
    std::vector<SourceViolation> violations{};
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
        if (!entry.is_regular_file() || !isScannedSourceFile(entry.path())) {
            continue;
        }

        const std::string source = readTextFile(entry.path());
        for (const auto keyword : kForbiddenKeywords) {
            if (containsForbiddenExceptionKeyword(source, keyword)) {
                violations.push_back(SourceViolation{
                    .path = entry.path(),
                    .token = std::string(keyword),
                });
            }
        }
    }
    return violations;
}

} // namespace

TEST(SourceNoExceptionsGuardTest, EngineAndGameSourcesDoNotUseCppExceptions) {
    const std::filesystem::path project_root = std::filesystem::path{PROJECT_SOURCE_DIR};
    const std::vector<std::filesystem::path> roots{
        project_root / "src" / "engine",
        project_root / "src" / "game",
    };

    std::vector<SourceViolation> violations{};
    for (const auto& root : roots) {
        ASSERT_TRUE(std::filesystem::exists(root)) << root;
        auto root_violations = collectExceptionTokenViolations(root);
        violations.insert(violations.end(), root_violations.begin(), root_violations.end());
    }

    for (const auto& violation : violations) {
        ADD_FAILURE() << violation.path << " contains C++ exception token: " << violation.token;
    }
}

// NOLINTEND
