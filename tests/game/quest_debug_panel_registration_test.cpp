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

TEST(QuestDebugPanelRegistrationTest, GameSceneRegistersQuestDebugPanelWithQuestServicesGuard) {
    const std::filesystem::path scene_source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/game_scene.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(scene_source_path)) << scene_source_path;

    const std::string source = test_source_utils::readTextFile(scene_source_path);
    ASSERT_FALSE(source.empty()) << "无法读取: " << scene_source_path;

    const std::string register_block =
        test_source_utils::extractFunctionBlock(source, "bool GameScene::registerDebugPanels()");
    ASSERT_FALSE(register_block.empty());

    EXPECT_NE(source.find("#include \"game/debug/quest_debug_panel.h\""), std::string::npos);
    EXPECT_NE(register_block.find("if (services_->quest_catalog && services_->quest_turn_in_service)"), std::string::npos);
    EXPECT_NE(register_block.find("std::make_unique<game::debug::QuestDebugPanel>"), std::string::npos);
    EXPECT_NE(register_block.find("services_->quest_catalog.get()"), std::string::npos);
    EXPECT_NE(register_block.find("services_->item_catalog.get()"), std::string::npos);
    EXPECT_NE(register_block.find("services_->quest_turn_in_service.get()"), std::string::npos);
    EXPECT_NE(register_block.find("engine::debug::PanelCategory::Game"), std::string::npos);
}

TEST(QuestDebugPanelRegistrationTest, DebugBuildSourcesIncludeQuestDebugPanel) {
    const std::filesystem::path cmake_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/CMakeLists.txt").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(cmake_path)) << cmake_path;

    const std::string source = test_source_utils::readTextFile(cmake_path);
    ASSERT_FALSE(source.empty()) << "无法读取: " << cmake_path;

    EXPECT_NE(source.find("game/debug/quest_debug_panel.cpp"), std::string::npos);
}

} // namespace
} // namespace game::debug
// NOLINTEND
