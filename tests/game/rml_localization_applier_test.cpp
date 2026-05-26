#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <cctype>
#include <filesystem>
#include <fstream>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

namespace {

struct RmlI18nRef {
    std::filesystem::path path{};
    std::string key{};
};

struct RmlTemplateI18nViolation {
    std::filesystem::path path{};
    int line{0};
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

[[nodiscard]] int rmlTagDepthDelta(std::string_view line) {
    int delta = 0;
    std::size_t pos = 0;
    while ((pos = line.find('<', pos)) != std::string_view::npos) {
        if (pos + 1 >= line.size()) {
            break;
        }
        const char next = line[pos + 1];
        if (next == '!' || next == '?') {
            ++pos;
            continue;
        }
        const std::size_t close = line.find('>', pos + 1);
        if (close == std::string_view::npos) {
            break;
        }
        if (next == '/') {
            --delta;
        } else {
            std::size_t end = close;
            while (end > pos && std::isspace(static_cast<unsigned char>(line[end - 1]))) {
                --end;
            }
            if (end == pos || line[end - 1] != '/') {
                ++delta;
            }
        }
        pos = close + 1;
    }
    return delta;
}

[[nodiscard]] std::vector<RmlTemplateI18nViolation> collectDataForI18nViolations() {
    std::vector<RmlTemplateI18nViolation> violations{};
    const std::filesystem::path scenes_dir = projectRoot() / "ui" / "rmlui" / "scenes";
    for (const auto& entry : std::filesystem::recursive_directory_iterator(scenes_dir)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".rml") {
            continue;
        }

        std::istringstream lines{slurp(entry.path())};
        std::string line{};
        bool inside_template = false;
        int template_depth = 0;
        int line_number = 0;
        while (std::getline(lines, line)) {
            ++line_number;
            const bool starts_template = !inside_template && line.find("data-for=") != std::string::npos;
            if (starts_template) {
                inside_template = true;
                template_depth = 0;
            }

            if (inside_template && line.find("data-i18n") != std::string::npos) {
                violations.push_back(RmlTemplateI18nViolation{.path = entry.path(), .line = line_number});
            }

            if (inside_template) {
                template_depth += rmlTagDepthDelta(line);
                if (template_depth <= 0) {
                    inside_template = false;
                }
            }
        }
    }
    return violations;
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
             "options.damage_popups",
             "options.enemy_hp_bar",
             "options.cursor_memory",
             "options.battle_speed",
         }) {
        EXPECT_TRUE(keys.contains(key)) << "inventory_menu.rml should keep data-i18n=" << key;
    }
    EXPECT_FALSE(keys.contains("options.language"))
        << "Language switching lives in pause_menu.rml so it is available from TitleScene.";
}

TEST(RmlLocalizationApplierSourceTest, CoreScenesKeepExpectedLocalizedStaticRefs) {
    const std::vector<RmlI18nRef> refs = collectRmlI18nRefs();
    std::set<std::string> keys{};
    for (const auto& ref : refs) {
        keys.insert(ref.key);
    }

    for (const std::string& key : {
             "appearance.preview",
             "appearance.random",
             "appearance.reset",
             "common.back",
             "common.confirm",
             "common.exp",
             "common.gold",
             "common.load",
             "common.menu",
             "common.ok",
             "common.save",
             "pause.resume",
             "pause.back_to_title",
             "options.language",
             "quest_offer.accept",
             "quest_offer.decline",
             "quest_offer.objective",
             "quest_offer.reward",
             "recruit_offer.join",
             "recruit_offer.not_now",
             "rest.title",
             "shop.category.consumable",
             "shop.category.equipment",
             "shop.detail.after",
             "shop.detail.price",
             "shop.detail.total",
             "shop.leave",
             "shop.mode.buy",
             "shop.mode.sell",
             "title.exit",
             "title.start",
         }) {
        EXPECT_TRUE(keys.contains(key)) << "Core RML should keep data-i18n marker " << key;
    }
}

TEST(RmlLocalizationApplierSourceTest, DataForTemplatesDoNotContainDataI18nMarkers) {
    const std::vector<RmlTemplateI18nViolation> violations = collectDataForI18nViolations();
    for (const auto& violation : violations) {
        ADD_FAILURE() << violation.path << ':' << violation.line
                      << " has data-i18n inside a data-for template; bind localized text through the view model instead";
    }
}
