// NOLINTBEGIN
#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

#include "../engine/render/test_source_utils.h"

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

namespace {

[[nodiscard]] std::vector<std::filesystem::path> collectFiles(const std::filesystem::path& root,
                                                              std::string_view extension) {
    std::vector<std::filesystem::path> files;
    if (!std::filesystem::exists(root)) {
        return files;
    }

    for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        if (entry.path().extension() == extension) {
            files.push_back(entry.path().lexically_normal());
        }
    }

    std::sort(files.begin(), files.end());
    return files;
}

} // namespace

namespace game::ui {
namespace {

TEST(RmlUiArchitectureRegressionTest, ProductionRmlUsesDataEventAndSharedThemeFiles) {
    const std::filesystem::path project_root = std::filesystem::path{PROJECT_SOURCE_DIR}.lexically_normal();
    const std::vector<std::filesystem::path> roots = {
        (project_root / "ui/rmlui/scenes").lexically_normal(),
        (project_root / "ui/rmlui/hud").lexically_normal(),
        (project_root / "ui/rmlui/overlay").lexically_normal(),
    };

    std::vector<std::filesystem::path> rml_files;
    for (const auto& root : roots) {
        const auto files = collectFiles(root, ".rml");
        rml_files.insert(rml_files.end(), files.begin(), files.end());
    }

    ASSERT_FALSE(rml_files.empty());

    for (const auto& path : rml_files) {
        const std::string source = test_source_utils::readTextFile(path);
        ASSERT_FALSE(source.empty()) << "无法读取: " << path;
        EXPECT_EQ(source.find("data-command="), std::string::npos)
            << "Production RML should not fall back to data-command: " << path;
        EXPECT_NE(source.find("reset.rcss"), std::string::npos)
            << "Production RML should include the shared reset theme: " << path;

        const std::string normalized = path.generic_string();
        const bool should_be_interactive =
            normalized.ends_with("/scenes/title.rml") ||
            normalized.ends_with("/scenes/pause_menu.rml") ||
            normalized.ends_with("/scenes/rest_dialog.rml") ||
            normalized.ends_with("/scenes/recruit_offer.rml") ||
            normalized.ends_with("/scenes/save_slot_select.rml") ||
            normalized.ends_with("/scenes/battle.rml") ||
            normalized.ends_with("/scenes/inventory_menu.rml") ||
            normalized.ends_with("/hud/game_overlay.rml") ||
            normalized.ends_with("/hud/hotbar.rml");

        if (should_be_interactive) {
            EXPECT_NE(source.find("data-event-"), std::string::npos)
                << "Interactive production RML should expose data-event bindings: " << path;
        }
    }
}

TEST(RmlUiArchitectureRegressionTest, RuntimeSourceDoesNotDependOnRemovedEventBridge) {
    const std::filesystem::path project_root = std::filesystem::path{PROJECT_SOURCE_DIR}.lexically_normal();
    const std::filesystem::path src_root = (project_root / "src").lexically_normal();

    const auto source_files = collectFiles(src_root, ".h");
    const auto cpp_files = collectFiles(src_root, ".cpp");

    ASSERT_FALSE(source_files.empty());
    ASSERT_FALSE(cpp_files.empty());

    const std::filesystem::path removed_header =
        (project_root / "src/engine/ui/rmlui/rml_event_bridge.h").lexically_normal();
    const std::filesystem::path removed_source =
        (project_root / "src/engine/ui/rmlui/rml_event_bridge.cpp").lexically_normal();
    EXPECT_FALSE(std::filesystem::exists(removed_header));
    EXPECT_FALSE(std::filesystem::exists(removed_source));

    for (const auto& path : source_files) {
        const std::string source = test_source_utils::readTextFile(path);
        ASSERT_FALSE(source.empty()) << "无法读取: " << path;
        EXPECT_EQ(source.find("RmlEventBridge"), std::string::npos) << path;
        EXPECT_EQ(source.find("rml_event_bridge"), std::string::npos) << path;
    }
    for (const auto& path : cpp_files) {
        const std::string source = test_source_utils::readTextFile(path);
        ASSERT_FALSE(source.empty()) << "无法读取: " << path;
        EXPECT_EQ(source.find("RmlEventBridge"), std::string::npos) << path;
        EXPECT_EQ(source.find("rml_event_bridge"), std::string::npos) << path;
    }
}

TEST(RmlUiArchitectureRegressionTest, RestDialogExposesRecoveryPreviewBindings) {
    const std::filesystem::path project_root = std::filesystem::path{PROJECT_SOURCE_DIR}.lexically_normal();
    const std::filesystem::path rest_rml_path = (project_root / "ui/rmlui/scenes/rest_dialog.rml").lexically_normal();
    const std::filesystem::path rest_scene_path =
        (project_root / "src/game/scene/rest_dialog_scene.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(rest_rml_path)) << rest_rml_path;
    ASSERT_TRUE(std::filesystem::exists(rest_scene_path)) << rest_scene_path;

    const std::string rml_source = test_source_utils::readTextFile(rest_rml_path);
    ASSERT_FALSE(rml_source.empty()) << "无法读取: " << rest_rml_path;
    EXPECT_NE(rml_source.find("recovery_summary_text"), std::string::npos);
    EXPECT_NE(rml_source.find("has_recovery_members"), std::string::npos);
    EXPECT_NE(rml_source.find("recovery_empty_text"), std::string::npos);
    EXPECT_NE(rml_source.find("data-for=\"member : recovery_members\""), std::string::npos);
    EXPECT_NE(rml_source.find("member.display_name"), std::string::npos);
    EXPECT_NE(rml_source.find("member.hp_label_text"), std::string::npos);
    EXPECT_NE(rml_source.find("member.hp_current_text"), std::string::npos);
    EXPECT_NE(rml_source.find("member.hp_after_text"), std::string::npos);
    EXPECT_NE(rml_source.find("member.mp_label_text"), std::string::npos);
    EXPECT_NE(rml_source.find("member.mp_current_text"), std::string::npos);
    EXPECT_NE(rml_source.find("member.mp_after_text"), std::string::npos);

    const std::string scene_source = test_source_utils::readTextFile(rest_scene_path);
    ASSERT_FALSE(scene_source.empty()) << "无法读取: " << rest_scene_path;
    EXPECT_NE(scene_source.find("RegisterArray<decltype(recovery_members_)>()"), std::string::npos);
    EXPECT_NE(scene_source.find("RegisterMember(\"hp_label_text\""), std::string::npos);
    EXPECT_NE(scene_source.find("RegisterMember(\"mp_label_text\""), std::string::npos);
    EXPECT_NE(scene_source.find("refreshRecoveryPreview();"), std::string::npos);
    EXPECT_NE(scene_source.find("RestConfirmRequest"), std::string::npos);
    EXPECT_NE(scene_source.find("sink<game::defs::LanguageChangedEvent>()"), std::string::npos);
    EXPECT_NE(scene_source.find("\"rest.summary.full_recovery\""), std::string::npos);
    EXPECT_NE(scene_source.find("\"rest.summary.recovery_percent\""), std::string::npos);
    EXPECT_NE(scene_source.find("tryLocalize(localization_, member.display_name_key)"), std::string::npos);
}

TEST(RmlUiArchitectureRegressionTest, BattleSceneUsesPlainButtonsAndDivBars) {
    const std::filesystem::path project_root = std::filesystem::path{PROJECT_SOURCE_DIR}.lexically_normal();
    const std::filesystem::path battle_rml_path = (project_root / "ui/rmlui/scenes/battle.rml").lexically_normal();
    const std::filesystem::path battle_rcss_path = (project_root / "ui/rmlui/scenes/battle.rcss").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(battle_rml_path)) << battle_rml_path;
    ASSERT_TRUE(std::filesystem::exists(battle_rcss_path)) << battle_rcss_path;

    const std::string rml = test_source_utils::readTextFile(battle_rml_path);
    const std::string rcss = test_source_utils::readTextFile(battle_rcss_path);
    ASSERT_FALSE(rml.empty());
    ASSERT_FALSE(rcss.empty());

    EXPECT_EQ(rml.find("tf-button-primary"), std::string::npos);
    EXPECT_EQ(rml.find("tf-button-secondary"), std::string::npos);
    EXPECT_EQ(rml.find("<progress"), std::string::npos);
    EXPECT_NE(rml.find("battle-text-button"), std::string::npos);
    EXPECT_NE(rml.find("id=\"battle-top-status\""), std::string::npos);
    EXPECT_NE(rml.find("battle-party-body"), std::string::npos);
    EXPECT_NE(rml.find("data-style-decorator=\"member.portrait_decorator\""), std::string::npos);
    EXPECT_NE(rml.find("data-style-width=\"member.hp_ratio_percent\""), std::string::npos);
    EXPECT_NE(rml.find("data-style-width=\"member.mp_ratio_percent\""), std::string::npos);
    EXPECT_EQ(rml.find("battle-menu-title"), std::string::npos);
    EXPECT_EQ(rml.find("battle-menu-hint"), std::string::npos);
    EXPECT_EQ(rcss.find("ninepatch"), std::string::npos);
}

TEST(RmlUiArchitectureRegressionTest, DialogueBoxUsesPortraitThemeAndRmlUiSafeRcss) {
    const std::filesystem::path project_root = std::filesystem::path{PROJECT_SOURCE_DIR}.lexically_normal();
    const std::filesystem::path rml_path = (project_root / "ui/rmlui/hud/dialogue_box.rml").lexically_normal();
    const std::filesystem::path rcss_path = (project_root / "ui/rmlui/hud/dialogue_box.rcss").lexically_normal();
    const std::filesystem::path spritesheet_path = (project_root / "ui/rmlui/theme/spritesheet.rcss").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(rml_path)) << rml_path;
    ASSERT_TRUE(std::filesystem::exists(rcss_path)) << rcss_path;
    ASSERT_TRUE(std::filesystem::exists(spritesheet_path)) << spritesheet_path;

    const std::string rml = test_source_utils::readTextFile(rml_path);
    const std::string rcss = test_source_utils::readTextFile(rcss_path);
    const std::string spritesheet = test_source_utils::readTextFile(spritesheet_path);
    ASSERT_FALSE(rml.empty());
    ASSERT_FALSE(rcss.empty());
    ASSERT_FALSE(spritesheet.empty());

    EXPECT_NE(rml.find("../theme/portrait.rcss"), std::string::npos);
    EXPECT_NE(rml.find("../theme/overlay_scene.rcss"), std::string::npos);
    EXPECT_NE(rml.find("dialogue-box-bg"), std::string::npos);
    EXPECT_NE(rml.find("dialogue-box-content"), std::string::npos);
    EXPECT_NE(rml.find("id=\"dialogue-box-portrait-frame\" class=\"tf-scene-card-frame\""), std::string::npos);
    EXPECT_NE(rml.find("dialogue-box-portrait"), std::string::npos);
    EXPECT_NE(rcss.find("body, div, h1, h2, h3, h4, p, hr"), std::string::npos);
    EXPECT_NE(rcss.find("bottom: 0dp;"), std::string::npos);
    EXPECT_NE(rcss.find("align-items: center;"), std::string::npos);
    EXPECT_NE(rcss.find("width: 600dp;"), std::string::npos);
    EXPECT_NE(rcss.find("height: 92dp;"), std::string::npos);
    EXPECT_NE(rcss.find("width: 584dp;"), std::string::npos);
    EXPECT_NE(rcss.find("height: 76dp;"), std::string::npos);
    EXPECT_NE(rcss.find("#dialogue-box-speaker {\n    height: 16dp;\n    margin-bottom: 4dp;\n    color: #fce97fff;\n    font-size: 1rem;\n    line-height: 16dp;"),
              std::string::npos);
    EXPECT_NE(rcss.find("#dialogue-box-text {\n    color: #f7f7f7ff;\n    font-size: 1rem;\n    line-height: 16dp;"),
              std::string::npos);
    EXPECT_EQ(rcss.find("font-size: 0.6875rem;"), std::string::npos);
    EXPECT_NE(rcss.find("hud-dialogue-bg"), std::string::npos);
    EXPECT_NE(rcss.find("image-color: #ffffffff;"), std::string::npos);
    EXPECT_NE(rcss.find("background-color: #00000000;"), std::string::npos);
    EXPECT_NE(rcss.find("opacity: 0.92;"), std::string::npos);
    EXPECT_NE(spritesheet.find("hud-dialogue-bg"), std::string::npos);
    EXPECT_NE(spritesheet.find("hud-dialogue-bg:       0px 48px 48px 48px;"), std::string::npos);
    EXPECT_EQ(spritesheet.find(std::string{"dialogue-"} + "bubble-bg"), std::string::npos);
    EXPECT_EQ(rcss.find("solid"), std::string::npos);
    EXPECT_EQ(rcss.find("padding: 2dp;"), std::string::npos);
    EXPECT_EQ(rcss.find("border: 1dp #565f89;"), std::string::npos);
    EXPECT_EQ(rcss.find("background-color: #16161eff;"), std::string::npos);
    EXPECT_EQ(rcss.find("left: 0; right:"), std::string::npos);
}

} // namespace
} // namespace game::ui
// NOLINTEND
