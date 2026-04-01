// NOLINTBEGIN
#include <gtest/gtest.h>

#include <filesystem>
#include <string>

#include "../engine/render/test_source_utils.h"

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

namespace game::scene {
namespace {

TEST(InventoryMenuSceneSlotGridRegistrationTest, SharedSlotVectorTypeIsRegisteredOnlyOnce) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/inventory_menu_scene.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = test_source_utils::readTextFile(source_path);
    ASSERT_FALSE(source.empty()) << "无法读取: " << source_path;

    const std::string init_ui_block =
        test_source_utils::extractFunctionBlock(source, "bool InventoryMenuScene::initUI()");
    ASSERT_FALSE(init_ui_block.empty());

    EXPECT_EQ(test_source_utils::countOccurrences(init_ui_block, "RegisterArray<decltype(backpack_slots_)>()"), 1U)
        << "InventoryMenuScene should register the shared SlotGrid vector array type exactly once.";
    EXPECT_EQ(test_source_utils::countOccurrences(init_ui_block, "RegisterArray<decltype(hotbar_slots_)>()"), 0U)
        << "Hotbar slots reuse the same vector<SlotGridViewModel> array type and should not register it again.";
    EXPECT_NE(init_ui_block.find("Bind(\"backpack_slots\", &backpack_slots_)"), std::string::npos);
    EXPECT_NE(init_ui_block.find("Bind(\"hotbar_slots\", &hotbar_slots_)"), std::string::npos);
}

} // namespace
} // namespace game::scene
// NOLINTEND
