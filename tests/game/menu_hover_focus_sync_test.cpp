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

TEST(MenuHoverFocusSyncTest, InventoryMenuUsesDedicatedSceneAndRemovesHoverListenerBeforeUnload) {
    const auto scene_path = projectPath("src/game/scene/game_scene.cpp");
    const auto ui_path = projectPath("src/game/ui/inventory_menu_ui.cpp");
    const auto rml_path = projectPath("ui/rmlui/scenes/inventory_menu.rml");
    ASSERT_TRUE(std::filesystem::exists(scene_path)) << scene_path;
    ASSERT_TRUE(std::filesystem::exists(ui_path)) << ui_path;
    ASSERT_TRUE(std::filesystem::exists(rml_path)) << rml_path;

    const std::string scene_source = readTextFile(scene_path);
    const std::string ui_source = readTextFile(ui_path);
    const std::string rml_source = readTextFile(rml_path);
    ASSERT_FALSE(scene_source.empty()) << scene_path;
    ASSERT_FALSE(ui_source.empty()) << ui_path;
    ASSERT_FALSE(rml_source.empty()) << rml_path;

    EXPECT_NE(scene_source.find("InventoryMenuScene"), std::string::npos);
    EXPECT_NE(scene_source.find("requestPushScene(std::move(menu));"), std::string::npos);
    EXPECT_EQ(scene_source.find("inventory_ui_"), std::string::npos);

    EXPECT_NE(ui_source.find("HoverFocusSyncListener"), std::string::npos);
    EXPECT_NE(ui_source.find("AddEventListener(\"mouseover\""), std::string::npos);
    EXPECT_NE(ui_source.find("RemoveEventListener(\"mouseover\""), std::string::npos);
    EXPECT_NE(ui_source.find("element->HasAttribute(\"data-for\")"), std::string::npos);

    const std::size_t remove_events_pos = ui_source.find("removeEventListeners();");
    const std::size_t unload_pos = ui_source.find("layer_.unloadDocument(document_);");
    ASSERT_NE(remove_events_pos, std::string::npos) << ui_path;
    ASSERT_NE(unload_pos, std::string::npos) << ui_path;
    EXPECT_LT(remove_events_pos, unload_pos) << ui_path;

    EXPECT_NE(rml_source.find("inventory-menu-slot-button inventory-menu-focusable"), std::string::npos);
    EXPECT_NE(rml_source.find("inventory-menu-action-button inventory-menu-focusable"), std::string::npos);
    EXPECT_NE(rml_source.find("data-attrif-disabled=\"action_menu_open\""), std::string::npos);
    EXPECT_NE(rml_source.find("data-visible=\"action_menu_open\""), std::string::npos);
    EXPECT_NE(rml_source.find("data-event-dragstart=\"slot_drag_start(slot.slot_index)\""), std::string::npos);
    EXPECT_NE(rml_source.find("data-event-dragdrop=\"slot_drag_drop(slot.slot_index)\""), std::string::npos);
    EXPECT_NE(rml_source.find("data-event-dragend=\"slot_drag_end(slot.slot_index)\""), std::string::npos);
}

TEST(MenuHoverFocusSyncTest, HoverFocusSyncListenerOnlyTargetsButtonsAndMenuBodiesEnableNavigation) {
    const auto listener_path = projectPath("src/engine/ui/rmlui/hover_focus_sync_listener.cpp");
    ASSERT_TRUE(std::filesystem::exists(listener_path)) << listener_path;

    const std::string listener_source = readTextFile(listener_path);
    ASSERT_FALSE(listener_source.empty());

    EXPECT_NE(listener_source.find("GetTagName() == \"button\""), std::string::npos);
    EXPECT_NE(listener_source.find("IsVisible(true)"), std::string::npos);
    EXPECT_NE(listener_source.find("getFocusedElement() == element"), std::string::npos);

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
