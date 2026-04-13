// NOLINTBEGIN
#include <gtest/gtest.h>

#include <filesystem>
#include <string>

#include "../engine/render/test_source_utils.h"

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

namespace game::debug {
namespace {

TEST(ShopDebugPanelRegistrationTest, GameSceneRegistersShopDebugPanelWithShopServicesGuard) {
    const std::filesystem::path scene_source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/game_scene.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(scene_source_path)) << scene_source_path;

    const std::string source = test_source_utils::readTextFile(scene_source_path);
    ASSERT_FALSE(source.empty()) << "无法读取: " << scene_source_path;

    const std::string register_block =
        test_source_utils::extractFunctionBlock(source, "bool GameScene::registerDebugPanels()");
    ASSERT_FALSE(register_block.empty());

    EXPECT_NE(source.find("#include \"game/debug/shop_debug_panel.h\""), std::string::npos);
    EXPECT_NE(register_block.find("if (services_->shop_catalog && services_->shop_transaction_service)"), std::string::npos);
    EXPECT_NE(register_block.find("std::make_unique<game::debug::ShopDebugPanel>"), std::string::npos);
    EXPECT_NE(register_block.find("services_->shop_catalog.get()"), std::string::npos);
    EXPECT_NE(register_block.find("services_->item_catalog.get()"), std::string::npos);
    EXPECT_NE(register_block.find("services_->shop_transaction_service.get()"), std::string::npos);
    EXPECT_NE(register_block.find("engine::debug::PanelCategory::Game"), std::string::npos);
}

TEST(ShopDebugPanelRegistrationTest, DebugBuildSourcesIncludeShopDebugPanelAndHelpers) {
    const std::filesystem::path cmake_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/CMakeLists.txt").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(cmake_path)) << cmake_path;

    const std::string source = test_source_utils::readTextFile(cmake_path);
    ASSERT_FALSE(source.empty()) << "无法读取: " << cmake_path;

    EXPECT_NE(source.find("game/debug/shop_debug_panel.cpp"), std::string::npos);
    EXPECT_NE(source.find("game/debug/shop_debug_panel_helpers.cpp"), std::string::npos);
}

TEST(ShopDebugPanelRegistrationTest, ShopDebugPanelComputesPreviewInsideActiveTabWithoutForcedSelection) {
    const std::filesystem::path panel_source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/debug/shop_debug_panel.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(panel_source_path)) << panel_source_path;

    const std::string source = test_source_utils::readTextFile(panel_source_path);
    ASSERT_FALSE(source.empty()) << "无法读取: " << panel_source_path;

    const std::size_t buy_tab_pos = source.find("if (ImGui::BeginTabItem(\"Buy\", nullptr))");
    const std::size_t sell_tab_pos = source.find("if (ImGui::BeginTabItem(\"Sell\", nullptr))");
    const std::size_t buy_preview_pos = source.find("const auto buy_preview =");
    const std::size_t sell_preview_pos = source.find("const auto sell_preview =");

    EXPECT_NE(buy_tab_pos, std::string::npos);
    EXPECT_NE(sell_tab_pos, std::string::npos);
    EXPECT_NE(buy_preview_pos, std::string::npos);
    EXPECT_NE(sell_preview_pos, std::string::npos);
    EXPECT_GT(buy_preview_pos, buy_tab_pos);
    EXPECT_GT(sell_preview_pos, sell_tab_pos);
    EXPECT_EQ(source.find("ImGuiTabItemFlags_SetSelected"), std::string::npos);
}

} // namespace
} // namespace game::debug
// NOLINTEND
