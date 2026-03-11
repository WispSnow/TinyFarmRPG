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
    const auto overlay_path = projectPath("ui/rmlui/hud/game_overlay.rml");
    ASSERT_TRUE(std::filesystem::exists(game_scene_path)) << game_scene_path;
    ASSERT_TRUE(std::filesystem::exists(overlay_path)) << overlay_path;

    const std::string game_scene_source = readTextFile(game_scene_path);
    const std::string overlay_source = readTextFile(overlay_path);
    ASSERT_FALSE(game_scene_source.empty());
    ASSERT_FALSE(overlay_source.empty());

    EXPECT_NE(game_scene_source.find("Bind(\"show_prompt_bar\""), std::string::npos);
    EXPECT_NE(game_scene_source.find("onAction(\"toggle_prompt_bar\"_hs)"), std::string::npos);
    EXPECT_NE(game_scene_source.find("markDirty(\"show_prompt_bar\")"), std::string::npos);
    EXPECT_NE(overlay_source.find("data-if=\"show_prompt_bar\""), std::string::npos);
}

TEST(MenuHoverFocusSyncTest, ScenesRegisterHoverFocusSyncAndRemoveListenersBeforeUnload) {
    const std::array<std::filesystem::path, 3> scene_paths{
        projectPath("src/game/scene/title_scene.cpp"),
        projectPath("src/game/scene/pause_menu_scene.cpp"),
        projectPath("src/game/scene/save_slot_select_scene.cpp"),
    };

    for (const auto& path : scene_paths) {
        ASSERT_TRUE(std::filesystem::exists(path)) << path;
        const std::string source = readTextFile(path);
        ASSERT_FALSE(source.empty()) << path;

        EXPECT_NE(source.find("HoverFocusSyncListener"), std::string::npos) << path;
        EXPECT_NE(source.find("AddEventListener(\"mouseover\""), std::string::npos) << path;
        EXPECT_NE(source.find("RemoveEventListener(\"mouseover\""), std::string::npos) << path;

        const std::size_t remove_events_pos = source.find("removeEventListeners();");
        const std::size_t unload_pos = source.find("unloadAllRmlDocuments();");
        ASSERT_NE(remove_events_pos, std::string::npos) << path;
        ASSERT_NE(unload_pos, std::string::npos) << path;
        EXPECT_LT(remove_events_pos, unload_pos) << path;
    }
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
