// NOLINTBEGIN
#include <gtest/gtest.h>

#include <filesystem>
#include <string>

#include "../render/test_source_utils.h"

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

namespace engine::core {
namespace {

TEST(GameAppUiNavigationSourceTest, GameAppNoLongerBindsMenuActionsDirectlyToRmlUiRuntime) {
    const std::filesystem::path project_root = std::filesystem::path{PROJECT_SOURCE_DIR}.lexically_normal();
    const std::filesystem::path header_path = (project_root / "src/engine/core/game_app.h").lexically_normal();
    const std::filesystem::path source_path = (project_root / "src/engine/core/game_app.cpp").lexically_normal();
    const std::filesystem::path removed_header =
        (project_root / "src/engine/ui/ui_navigation_controller.h").lexically_normal();
    const std::filesystem::path removed_source =
        (project_root / "src/engine/ui/ui_navigation_controller.cpp").lexically_normal();

    ASSERT_TRUE(std::filesystem::exists(header_path)) << header_path;
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;
    EXPECT_FALSE(std::filesystem::exists(removed_header));
    EXPECT_FALSE(std::filesystem::exists(removed_source));

    const std::string header = test_source_utils::readTextFile(header_path);
    const std::string source = test_source_utils::readTextFile(source_path);
    ASSERT_FALSE(header.empty()) << "无法读取: " << header_path;
    ASSERT_FALSE(source.empty()) << "无法读取: " << source_path;

    EXPECT_EQ(header.find("UINavigationController"), std::string::npos);
    EXPECT_EQ(source.find("UINavigationController"), std::string::npos);
    EXPECT_EQ(header.find("initMenuNavigationBindings"), std::string::npos);
    EXPECT_EQ(source.find("bool GameApp::initMenuNavigationBindings()"), std::string::npos);
    EXPECT_EQ(source.find("disconnectMenuNavigationBindings"), std::string::npos);

    EXPECT_EQ(source.find("input_manager_->onAction(\"menu_up\"_hs).connect<&GameApp::onMenuNavigateUpPressed>(this);"),
              std::string::npos);
    EXPECT_EQ(
        source.find("input_manager_->onAction(\"menu_down\"_hs).connect<&GameApp::onMenuNavigateDownPressed>(this);"),
        std::string::npos);
    EXPECT_EQ(
        source.find("input_manager_->onAction(\"menu_left\"_hs).connect<&GameApp::onMenuNavigateLeftPressed>(this);"),
        std::string::npos);
    EXPECT_EQ(
        source.find("input_manager_->onAction(\"menu_right\"_hs).connect<&GameApp::onMenuNavigateRightPressed>(this);"),
        std::string::npos);
    EXPECT_EQ(source.find("input_manager_->onAction(\"menu_confirm\"_hs).connect<&GameApp::onMenuConfirmPressed>(this);"),
              std::string::npos);
}

TEST(GameAppUiNavigationSourceTest, DirectMenuHandlersAndRuntimeNavigationCallsAreRemoved) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/core/game_app.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = test_source_utils::readTextFile(source_path);
    ASSERT_FALSE(source.empty()) << "无法读取: " << source_path;

    EXPECT_EQ(source.find("bool GameApp::onMenuNavigateUpPressed()"), std::string::npos);
    EXPECT_EQ(source.find("bool GameApp::onMenuNavigateDownPressed()"), std::string::npos);
    EXPECT_EQ(source.find("bool GameApp::onMenuNavigateLeftPressed()"), std::string::npos);
    EXPECT_EQ(source.find("bool GameApp::onMenuNavigateRightPressed()"), std::string::npos);
    EXPECT_EQ(source.find("bool GameApp::onMenuConfirmPressed()"), std::string::npos);

    EXPECT_EQ(source.find("rmlui_runtime_->navigateUp();"), std::string::npos);
    EXPECT_EQ(source.find("rmlui_runtime_->navigateDown();"), std::string::npos);
    EXPECT_EQ(source.find("rmlui_runtime_->navigateLeft();"), std::string::npos);
    EXPECT_EQ(source.find("rmlui_runtime_->navigateRight();"), std::string::npos);
    EXPECT_EQ(source.find("rmlui_runtime_->confirmFocusedElement();"), std::string::npos);
}

} // namespace
} // namespace engine::core
// NOLINTEND
