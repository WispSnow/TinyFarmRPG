#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

namespace {

[[nodiscard]] std::filesystem::path projectRoot() {
    return std::filesystem::path{PROJECT_SOURCE_DIR};
}

[[nodiscard]] nlohmann::json parseJsonFile(const std::filesystem::path& path) {
    std::ifstream file(path);
    EXPECT_TRUE(file.is_open()) << "Failed to open " << path;
    std::stringstream buffer;
    buffer << file.rdbuf();
    nlohmann::json root = nlohmann::json::parse(buffer.str(), nullptr, false);
    EXPECT_FALSE(root.is_discarded()) << "Invalid JSON: " << path;
    return root;
}

[[nodiscard]] std::filesystem::path resolveProjectPath(const std::string& path) {
    const std::filesystem::path raw{path};
    return raw.is_absolute() ? raw : projectRoot() / raw;
}

[[nodiscard]] std::set<std::string> keysFor(const nlohmann::json& table) {
    std::set<std::string> keys{};
    if (!table.is_object()) {
        return keys;
    }
    for (auto it = table.begin(); it != table.end(); ++it) {
        keys.insert(it.key());
    }
    return keys;
}

[[nodiscard]] std::vector<std::string> diffKeys(const std::set<std::string>& expected,
                                                const std::set<std::string>& actual) {
    std::vector<std::string> missing{};
    for (const auto& key : expected) {
        if (!actual.contains(key)) {
            missing.push_back(key);
        }
    }
    return missing;
}

} // namespace

TEST(I18nKeyParityTest, EveryLanguageHasFallbackKeySet) {
    const nlohmann::json manifest = parseJsonFile(projectRoot() / "assets" / "i18n" / "languages.json");
    ASSERT_TRUE(manifest.is_object());
    ASSERT_TRUE(manifest["languages"].is_array());

    const std::string fallback_tag = manifest.value("fallback", "en-US");
    std::set<std::string> fallback_keys{};
    std::vector<std::pair<std::string, std::set<std::string>>> language_keys{};

    for (const auto& language : manifest["languages"]) {
        ASSERT_TRUE(language.is_object());
        const std::string tag = language.value("tag", "");
        const std::string file = language.value("file", "");
        ASSERT_FALSE(tag.empty());
        ASSERT_FALSE(file.empty());

        const nlohmann::json table = parseJsonFile(resolveProjectPath(file));
        ASSERT_TRUE(table.is_object()) << file << " must be a flat object";
        const auto keys = keysFor(table);
        if (tag == fallback_tag) {
            fallback_keys = keys;
        }
        language_keys.emplace_back(tag, keys);
    }

    ASSERT_FALSE(fallback_keys.empty()) << "fallback language table must not be empty";
    for (const auto& [tag, keys] : language_keys) {
        EXPECT_TRUE(diffKeys(fallback_keys, keys).empty())
            << tag << " is missing keys from fallback language";
        EXPECT_TRUE(diffKeys(keys, fallback_keys).empty())
            << tag << " has keys that fallback language does not have";
    }
}

TEST(I18nKeyParityTest, EveryProjectMapHasDisplayNameKey) {
    const nlohmann::json english = parseJsonFile(projectRoot() / "assets" / "i18n" / "en-US.json");
    ASSERT_TRUE(english.is_object());

    const std::filesystem::path maps_dir = projectRoot() / "assets" / "maps";
    ASSERT_TRUE(std::filesystem::exists(maps_dir)) << maps_dir;

    std::vector<std::string> missing_keys{};
    for (const auto& entry : std::filesystem::directory_iterator{maps_dir}) {
        if (!entry.is_regular_file() || entry.path().extension() != ".tmj") {
            continue;
        }

        const std::string key = "map." + entry.path().stem().string() + ".name";
        if (!english.contains(key)) {
            missing_keys.push_back(key);
        }
    }

    EXPECT_TRUE(missing_keys.empty()) << "Missing map display name keys: " << [&] {
        std::ostringstream out;
        for (const std::string& key : missing_keys) {
            if (out.tellp() > 0) {
                out << ", ";
            }
            out << key;
        }
        return out.str();
    }();
}
