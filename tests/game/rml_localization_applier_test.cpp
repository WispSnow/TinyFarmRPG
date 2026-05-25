#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

namespace {

struct RmlI18nRef {
    std::filesystem::path path{};
    std::string key{};
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

[[nodiscard]] nlohmann::json parseJsonFile(const std::filesystem::path& path) {
    const std::string content = slurp(path);
    nlohmann::json root = nlohmann::json::parse(content, nullptr, false);
    EXPECT_FALSE(root.is_discarded()) << "Invalid JSON: " << path;
    return root;
}

[[nodiscard]] std::set<std::string> keySetFor(const std::filesystem::path& path) {
    const nlohmann::json table = parseJsonFile(path);
    EXPECT_TRUE(table.is_object()) << path << " must be a flat object";
    std::set<std::string> keys{};
    for (auto it = table.begin(); it != table.end(); ++it) {
        keys.insert(it.key());
    }
    return keys;
}

[[nodiscard]] std::vector<RmlI18nRef> collectRmlI18nRefs() {
    const std::regex attr_regex{R"(data-i18n(?:-title)?\s*=\s*["']([^"']+)["'])"};
    std::vector<RmlI18nRef> refs{};

    const std::filesystem::path scenes_dir = projectRoot() / "ui" / "rmlui" / "scenes";
    for (const auto& entry : std::filesystem::recursive_directory_iterator(scenes_dir)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".rml") {
            continue;
        }

        const std::string source = slurp(entry.path());
        for (std::sregex_iterator it(source.begin(), source.end(), attr_regex), end; it != end; ++it) {
            refs.push_back(RmlI18nRef{
                .path = entry.path(),
                .key = (*it)[1].str(),
            });
        }
    }
    return refs;
}

} // namespace

TEST(RmlLocalizationApplierSourceTest, DataI18nKeysExistInAllLanguageTables) {
    const std::vector<RmlI18nRef> refs = collectRmlI18nRefs();
    ASSERT_FALSE(refs.empty()) << "At least one RML data-i18n marker should exist";

    const std::set<std::string> en_keys = keySetFor(projectRoot() / "assets" / "i18n" / "en-US.json");
    const std::set<std::string> zh_keys = keySetFor(projectRoot() / "assets" / "i18n" / "zh-Hans.json");

    for (const auto& ref : refs) {
        EXPECT_TRUE(en_keys.contains(ref.key)) << ref.path << " references missing en-US key " << ref.key;
        EXPECT_TRUE(zh_keys.contains(ref.key)) << ref.path << " references missing zh-Hans key " << ref.key;
    }
}

TEST(RmlLocalizationApplierSourceTest, InventoryMenuKeepsExpectedLocalizedStaticRefs) {
    const std::vector<RmlI18nRef> refs = collectRmlI18nRefs();
    std::set<std::string> keys{};
    for (const auto& ref : refs) {
        if (ref.path.filename() == "inventory_menu.rml") {
            keys.insert(ref.key);
        }
    }

    for (const std::string& key : {
             "inventory.tab.inventory",
             "inventory.tab.equipment",
             "inventory.tab.quests",
             "inventory.tab.map",
             "inventory.tab.options",
             "inventory.action.sort",
             "inventory.action.trash",
             "inventory.equipment.no_candidates",
             "inventory.equipment.unequip",
             "inventory.quest.active_title",
             "inventory.quest.active_empty",
             "inventory.quest.completed_title",
             "inventory.quest.completed_empty",
             "inventory.map.empty",
             "options.language",
             "options.damage_popups",
             "options.enemy_hp_bar",
             "options.cursor_memory",
             "options.battle_speed",
         }) {
        EXPECT_TRUE(keys.contains(key)) << "inventory_menu.rml should keep data-i18n=" << key;
    }
}
