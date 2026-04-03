// NOLINTBEGIN
#include <gtest/gtest.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

namespace {

[[nodiscard]] std::string readTextFile(const std::filesystem::path& path) {
    std::ifstream file(path);
    EXPECT_TRUE(file.is_open()) << path;
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

[[nodiscard]] std::filesystem::path projectPath(std::string_view relative_path) {
    return (std::filesystem::path{PROJECT_SOURCE_DIR} / relative_path).lexically_normal();
}

} // namespace

namespace game::scene {
namespace {

TEST(MenuHoverFocusSyncTest, GameOverlayBindsPromptBarVisibilityAndToggleAction) {
    const auto game_scene_path = projectPath("src/game/scene/game_scene.cpp");
    const auto controller_path = projectPath("src/game/ui/game_scene_ui_controller.cpp");
    const auto overlay_path = projectPath("ui/rmlui/hud/game_overlay.rml");
    ASSERT_TRUE(std::filesystem::exists(game_scene_path)) << game_scene_path;
    ASSERT_TRUE(std::filesystem::exists(controller_path)) << controller_path;
    ASSERT_TRUE(std::filesystem::exists(overlay_path)) << overlay_path;

    const std::string game_scene_source = readTextFile(game_scene_path);
    const std::string controller_source = readTextFile(controller_path);
    const std::string overlay_source = readTextFile(overlay_path);
    ASSERT_FALSE(game_scene_source.empty());
    ASSERT_FALSE(controller_source.empty());
    ASSERT_FALSE(overlay_source.empty());

    EXPECT_NE(controller_source.find("Bind(\"show_prompt_bar\""), std::string::npos);
    EXPECT_NE(game_scene_source.find("onAction(\"toggle_prompt_bar\"_hs)"), std::string::npos);
    EXPECT_NE(controller_source.find("markDirty(\"show_prompt_bar\")"), std::string::npos);
    EXPECT_NE(overlay_source.find("data-if=\"show_prompt_bar\""), std::string::npos);
}

TEST(MenuHoverFocusSyncTest, ScenesNoLongerRegisterHoverFocusSyncAndStillCleanupViaShutdownUI) {
    const std::array<std::filesystem::path, 3> scene_paths{
        projectPath("src/game/scene/title_scene.cpp"),
        projectPath("src/game/scene/pause_menu_scene.cpp"),
        projectPath("src/game/scene/save_slot_select_scene.cpp"),
    };

    for (const auto& path : scene_paths) {
        ASSERT_TRUE(std::filesystem::exists(path)) << path;
        const std::string source = readTextFile(path);
        ASSERT_FALSE(source.empty()) << path;

        EXPECT_EQ(source.find("document_controller_.enableHoverFocusSync("), std::string::npos) << path;
        EXPECT_NE(source.find("document_controller_.unload();"), std::string::npos) << path;

        EXPECT_EQ(source.find("HoverFocusSyncListener"), std::string::npos) << path;
        EXPECT_EQ(source.find("registerTo("), std::string::npos) << path;
        EXPECT_EQ(source.find("unregisterAll()"), std::string::npos) << path;
    }
}

TEST(MenuHoverFocusSyncTest, SaveSlotSelectSceneNoLongerKeepsCustomFocusRecoveryLogic) {
    const auto source_path = projectPath("src/game/scene/save_slot_select_scene.cpp");
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = readTextFile(source_path);
    ASSERT_FALSE(source.empty());

    EXPECT_EQ(source.find("shouldSyncHoverFocus"), std::string::npos);
    EXPECT_EQ(source.find("focus_before_confirm_"), std::string::npos);
    EXPECT_NE(source.find("confirm_visible_"), std::string::npos);
}

TEST(MenuHoverFocusSyncTest, NavigationThemeRemainsAvailableAfterCustomFocusRemoval) {
    const auto listener_path = projectPath("src/engine/ui/rmlui/hover_focus_sync_listener.cpp");
    const auto nav_theme_path = projectPath("ui/rmlui/theme/nav.rcss");
    EXPECT_FALSE(std::filesystem::exists(listener_path));
    ASSERT_TRUE(std::filesystem::exists(nav_theme_path)) << nav_theme_path;

    const std::string nav_theme_source = readTextFile(nav_theme_path);
    ASSERT_FALSE(nav_theme_source.empty());

    EXPECT_NE(nav_theme_source.find(".tf-nav-root"), std::string::npos);
    EXPECT_NE(nav_theme_source.find("nav: auto;"), std::string::npos);

    const std::array<std::filesystem::path, 3> style_paths{
        projectPath("ui/rmlui/scenes/title.rml"),
        projectPath("ui/rmlui/scenes/pause_menu.rml"),
        projectPath("ui/rmlui/scenes/save_slot_select.rml"),
    };

    for (const auto& path : style_paths) {
        ASSERT_TRUE(std::filesystem::exists(path)) << path;
        const std::string source = readTextFile(path);
        ASSERT_FALSE(source.empty()) << path;
        EXPECT_NE(source.find("../theme/nav.rcss"), std::string::npos) << path;
        EXPECT_NE(source.find("tf-nav-root"), std::string::npos) << path;
    }
}

} // namespace
} // namespace game::scene
// NOLINTEND
