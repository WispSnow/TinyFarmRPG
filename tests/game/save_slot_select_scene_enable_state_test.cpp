// NOLINTBEGIN
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

namespace {

[[nodiscard]] std::string readTextFile(const std::filesystem::path& path) {
    std::ifstream file(path);
    EXPECT_TRUE(file.is_open()) << path;
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

} // namespace

namespace game::scene {
namespace {

TEST(SaveSlotSelectSceneEnableStateTest, RefreshSlotButtonsUsesSetEnabled) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/save_slot_select_scene.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = readTextFile(source_path);
    ASSERT_FALSE(source.empty());

    EXPECT_NE(source.find("slot.enabled = (mode_ == Mode::Save);"), std::string::npos)
        << "SaveSlotSelectScene should derive Empty slot enable state from current mode.";
    EXPECT_NE(source.find("data_bridge_.markDirty(\"slots\");"), std::string::npos)
        << "SaveSlotSelectScene should propagate slot enable state through data binding.";
}

} // namespace
} // namespace game::scene
// NOLINTEND
