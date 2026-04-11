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

TEST(BattleDebugPanelRegistrationTest, GameSceneRegistersBattleDebugPanelWithRpgCatalog) {
    const std::filesystem::path scene_source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/game_scene.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(scene_source_path)) << scene_source_path;

    const std::string source = test_source_utils::readTextFile(scene_source_path);
    ASSERT_FALSE(source.empty()) << "无法读取: " << scene_source_path;

    const std::string register_block =
        test_source_utils::extractFunctionBlock(source, "bool GameScene::registerDebugPanels()");
    ASSERT_FALSE(register_block.empty());

    EXPECT_NE(source.find("#include \"game/debug/battle_debug_panel.h\""), std::string::npos);
    EXPECT_NE(register_block.find("if (services_->rpg_catalog)"), std::string::npos);
    EXPECT_NE(register_block.find("std::make_unique<game::debug::BattleDebugPanel>"), std::string::npos);
    EXPECT_NE(register_block.find("services_->rpg_catalog.get()"), std::string::npos);
    EXPECT_NE(register_block.find("engine::debug::PanelCategory::Game"), std::string::npos);
}

TEST(BattleDebugPanelRegistrationTest, BattlePanelOwnsTroopSelectionAndPlayerPanelNoLongerStartsBattles) {
    const std::filesystem::path battle_panel_source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/debug/battle_debug_panel.cpp").lexically_normal();
    const std::filesystem::path player_panel_source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/debug/player_debug_panel.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(battle_panel_source_path)) << battle_panel_source_path;
    ASSERT_TRUE(std::filesystem::exists(player_panel_source_path)) << player_panel_source_path;

    const std::string battle_source = test_source_utils::readTextFile(battle_panel_source_path);
    const std::string player_source = test_source_utils::readTextFile(player_panel_source_path);
    ASSERT_FALSE(battle_source.empty()) << "无法读取: " << battle_panel_source_path;
    ASSERT_FALSE(player_source.empty()) << "无法读取: " << player_panel_source_path;

    EXPECT_NE(battle_source.find("listTroops()"), std::string::npos);
    EXPECT_NE(battle_source.find("selected_troop_id_"), std::string::npos);
    EXPECT_NE(battle_source.find("AddItemCommand{"), std::string::npos);
    EXPECT_NE(battle_source.find("EnterBattleCommand{"), std::string::npos);
    EXPECT_NE(battle_source.find(".troop_id = selected_troop_id_"), std::string::npos);
    EXPECT_EQ(battle_source.find("slot.item_id_ = potion_id;"), std::string::npos);
    EXPECT_EQ(battle_source.find("slot.count_ = 3;"), std::string::npos);

    EXPECT_EQ(player_source.find("Start Test Battle"), std::string::npos);
    EXPECT_EQ(player_source.find("EnterBattleCommand"), std::string::npos);
}

TEST(BattleDebugPanelRegistrationTest, DebugBuildSourcesIncludeBattleDebugPanel) {
    const std::filesystem::path cmake_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/CMakeLists.txt").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(cmake_path)) << cmake_path;

    const std::string source = test_source_utils::readTextFile(cmake_path);
    ASSERT_FALSE(source.empty()) << "无法读取: " << cmake_path;

    EXPECT_NE(source.find("game/debug/battle_debug_panel.cpp"), std::string::npos);
}

} // namespace
} // namespace game::debug
// NOLINTEND
