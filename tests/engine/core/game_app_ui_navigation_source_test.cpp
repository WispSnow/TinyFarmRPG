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

TEST(GameAppUiNavigationSourceTest, GameAppBindsMenuActionsDirectlyToRmlUiRuntime) {
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
    EXPECT_NE(header.find("initMenuNavigationBindings"), std::string::npos);
    EXPECT_NE(source.find("bool GameApp::initMenuNavigationBindings()"), std::string::npos);

    EXPECT_NE(source.find("input_manager_->onAction(\"menu_up\"_hs).connect<&GameApp::onMenuNavigateUpPressed>(this);"),
              std::string::npos);
    EXPECT_NE(
        source.find("input_manager_->onAction(\"menu_down\"_hs).connect<&GameApp::onMenuNavigateDownPressed>(this);"),
        std::string::npos);
    EXPECT_NE(
        source.find("input_manager_->onAction(\"menu_left\"_hs).connect<&GameApp::onMenuNavigateLeftPressed>(this);"),
        std::string::npos);
    EXPECT_NE(
        source.find("input_manager_->onAction(\"menu_right\"_hs).connect<&GameApp::onMenuNavigateRightPressed>(this);"),
        std::string::npos);
    EXPECT_NE(source.find("input_manager_->onAction(\"menu_confirm\"_hs).connect<&GameApp::onMenuConfirmPressed>(this);"),
              std::string::npos);
}

TEST(GameAppUiNavigationSourceTest, DirectMenuHandlersForwardToRmlUiRuntimeWithoutConsumingInput) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/core/game_app.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = test_source_utils::readTextFile(source_path);
    ASSERT_FALSE(source.empty()) << "无法读取: " << source_path;

    const std::string up_block = test_source_utils::extractFunctionBlock(source, "bool GameApp::onMenuNavigateUpPressed()");
    const std::string down_block =
        test_source_utils::extractFunctionBlock(source, "bool GameApp::onMenuNavigateDownPressed()");
    const std::string left_block =
        test_source_utils::extractFunctionBlock(source, "bool GameApp::onMenuNavigateLeftPressed()");
    const std::string right_block =
        test_source_utils::extractFunctionBlock(source, "bool GameApp::onMenuNavigateRightPressed()");
    const std::string confirm_block =
        test_source_utils::extractFunctionBlock(source, "bool GameApp::onMenuConfirmPressed()");

    ASSERT_FALSE(up_block.empty());
    ASSERT_FALSE(down_block.empty());
    ASSERT_FALSE(left_block.empty());
    ASSERT_FALSE(right_block.empty());
    ASSERT_FALSE(confirm_block.empty());

    EXPECT_NE(up_block.find("rmlui_runtime_->navigateUp();"), std::string::npos);
    EXPECT_NE(down_block.find("rmlui_runtime_->navigateDown();"), std::string::npos);
    EXPECT_NE(left_block.find("rmlui_runtime_->navigateLeft();"), std::string::npos);
    EXPECT_NE(right_block.find("rmlui_runtime_->navigateRight();"), std::string::npos);
    EXPECT_NE(confirm_block.find("rmlui_runtime_->confirmFocusedElement();"), std::string::npos);

    EXPECT_NE(up_block.find("return false;"), std::string::npos);
    EXPECT_NE(down_block.find("return false;"), std::string::npos);
    EXPECT_NE(left_block.find("return false;"), std::string::npos);
    EXPECT_NE(right_block.find("return false;"), std::string::npos);
    EXPECT_NE(confirm_block.find("return false;"), std::string::npos);
}

} // namespace
} // namespace engine::core
// NOLINTEND
