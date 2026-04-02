// NOLINTBEGIN
#include <gtest/gtest.h>

#include <filesystem>
#include <string>

#include "../render/test_source_utils.h"

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

namespace engine::ui {
namespace {

TEST(UiTypesSourceTest, SlotItemKeepsOnlyItemIdentityAndCount) {
    const std::filesystem::path header_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/ui/ui_types.h").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(header_path)) << header_path;

    const std::string source = test_source_utils::readTextFile(header_path);
    ASSERT_FALSE(source.empty()) << "无法读取: " << header_path;

    EXPECT_NE(source.find("struct SlotItem"), std::string::npos);
    EXPECT_NE(source.find("entt::id_type item_id"), std::string::npos);
    EXPECT_NE(source.find("int count"), std::string::npos);
    EXPECT_EQ(source.find("Image icon"), std::string::npos);
    EXPECT_EQ(source.find("#include \"engine/render/image.h\""), std::string::npos);
}

TEST(UiTypesSourceTest, HotbarSyncNoLongerFetchesLegacyImageIconsForSlotItems) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/ui/game_scene_ui_controller.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = test_source_utils::readTextFile(source_path);
    ASSERT_FALSE(source.empty()) << "无法读取: " << source_path;

    const std::string block =
        test_source_utils::extractFunctionBlock(source, "void GameSceneUiController::applyHotbarChanged(const game::defs::HotbarChanged& evt)");
    ASSERT_FALSE(block.empty());

    EXPECT_EQ(block.find("getItemIcon("), std::string::npos);
    EXPECT_NE(block.find("engine::ui::SlotItem{slot.item_id, slot.count}"), std::string::npos);
}

} // namespace
} // namespace engine::ui
// NOLINTEND
