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

TEST(GameSceneRuntimeAssemblyTest, UsesRuntimeAssemblerForInitPath) {
    const std::filesystem::path scene_source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/game_scene.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(scene_source_path)) << scene_source_path;

    const std::string scene_source = readTextFile(scene_source_path);
    ASSERT_FALSE(scene_source.empty());

    EXPECT_NE(scene_source.find("GameRuntimeAssembler::assembleServices"), std::string::npos);
    EXPECT_NE(scene_source.find("GameRuntimeAssembler::assembleSystems"), std::string::npos);
    EXPECT_EQ(scene_source.find("bool GameScene::initSystems()"), std::string::npos)
        << "GameScene should no longer own system construction implementation.";

}

TEST(GameSceneRuntimeAssemblyTest, RuntimeAssemblerSourceExists) {
    const std::filesystem::path assembler_source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/runtime/game_runtime_assembler.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(assembler_source_path)) << assembler_source_path;

    const std::string assembler_source = readTextFile(assembler_source_path);
    ASSERT_FALSE(assembler_source.empty());
    EXPECT_NE(assembler_source.find("bool GameRuntimeAssembler::assembleServices"), std::string::npos);
    EXPECT_NE(assembler_source.find("bool GameRuntimeAssembler::assembleSystems"), std::string::npos);
}

} // namespace
} // namespace game::scene
// NOLINTEND
