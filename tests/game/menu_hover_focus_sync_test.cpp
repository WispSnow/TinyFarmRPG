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

TEST(MenuHoverFocusSyncTest, ScenesRegisterHoverFocusSyncAndRemoveListenersBeforeUnload) {
    const std::array<std::filesystem::path, 3> scene_paths{
        projectPath("src/game/scene/title_scene.cpp"),
        projectPath("src/game/scene/pause_menu_scene.cpp"),
        projectPath("src/game/scene/save_slot_select_scene.cpp"),
    };
    const auto scene_base_path = projectPath("src/engine/scene/scene.cpp");
    ASSERT_TRUE(std::filesystem::exists(scene_base_path)) << scene_base_path;
    const std::string scene_base_source = readTextFile(scene_base_path);
    ASSERT_FALSE(scene_base_source.empty()) << scene_base_path;

    for (const auto& path : scene_paths) {
        ASSERT_TRUE(std::filesystem::exists(path)) << path;
        const std::string source = readTextFile(path);
        ASSERT_FALSE(source.empty()) << path;

        EXPECT_NE(source.find("HoverFocusSyncListener"), std::string::npos) << path;
        EXPECT_NE(source.find("registerTo("), std::string::npos) << path;
        EXPECT_NE(source.find("unregisterAll()"), std::string::npos) << path;
        EXPECT_NE(source.find("beforeUnloadOwnedRmlDocuments"), std::string::npos) << path;
        EXPECT_NE(source.find("afterUnloadOwnedRmlDocuments"), std::string::npos) << path;
    }

    const std::size_t before_hook_pos = scene_base_source.find("beforeUnloadOwnedRmlDocuments();");
    const std::size_t unload_pos = scene_base_source.find("unloadAllRmlDocuments();");
    const std::size_t after_hook_pos = scene_base_source.find("afterUnloadOwnedRmlDocuments();");
    ASSERT_NE(before_hook_pos, std::string::npos) << scene_base_path;
    ASSERT_NE(unload_pos, std::string::npos) << scene_base_path;
    ASSERT_NE(after_hook_pos, std::string::npos) << scene_base_path;
    EXPECT_LT(before_hook_pos, unload_pos) << scene_base_path;
    EXPECT_LT(unload_pos, after_hook_pos) << scene_base_path;
}

TEST(MenuHoverFocusSyncTest, SaveSlotSelectSceneGuardsHoverSyncWhenConfirmVisible) {
    const auto source_path = projectPath("src/game/scene/save_slot_select_scene.cpp");
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = readTextFile(source_path);
    ASSERT_FALSE(source.empty());

    EXPECT_NE(source.find("shouldSyncHoverFocus"), std::string::npos);
    EXPECT_NE(source.find("confirm_visible_"), std::string::npos);
    EXPECT_NE(source.find("save-slot-confirm-layer"), std::string::npos);
}

TEST(MenuHoverFocusSyncTest, HoverFocusSyncListenerOnlyTargetsButtonsAndMenuBodiesEnableNavigation) {
    const auto listener_path = projectPath("src/engine/ui/rmlui/hover_focus_sync_listener.cpp");
    ASSERT_TRUE(std::filesystem::exists(listener_path)) << listener_path;

    const std::string listener_source = readTextFile(listener_path);
    ASSERT_FALSE(listener_source.empty());

    EXPECT_NE(listener_source.find("GetTagName() == \"button\""), std::string::npos);
    EXPECT_NE(listener_source.find("IsVisible(true)"), std::string::npos);

    const std::array<std::filesystem::path, 3> style_paths{
        projectPath("ui/rmlui/scenes/title.rcss"),
        projectPath("ui/rmlui/scenes/pause_menu.rcss"),
        projectPath("ui/rmlui/scenes/save_slot_select.rcss"),
    };

    for (const auto& path : style_paths) {
        ASSERT_TRUE(std::filesystem::exists(path)) << path;
        const std::string source = readTextFile(path);
        ASSERT_FALSE(source.empty()) << path;
        EXPECT_NE(source.find("nav: auto;"), std::string::npos) << path;
    }
}

} // namespace
} // namespace game::scene
// NOLINTEND
