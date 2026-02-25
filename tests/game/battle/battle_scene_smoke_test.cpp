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

TEST(BattleSceneSmokeTest, ContainsStateMachineStages) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/battle_scene.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = readTextFile(source_path);
    ASSERT_FALSE(source.empty());

    EXPECT_NE(source.find("FlowState::WaitingForInput"), std::string::npos);
    EXPECT_NE(source.find("FlowState::ExecutingAction"), std::string::npos);
    EXPECT_NE(source.find("FlowState::AnimatingResult"), std::string::npos);
    EXPECT_NE(source.find("FlowState::CheckVictory"), std::string::npos);
    EXPECT_NE(source.find("FlowState::NextTurn"), std::string::npos);
    EXPECT_NE(source.find("FlowState::BattleEnd"), std::string::npos);
}

TEST(BattleSceneSmokeTest, EmitsBattleEndedEventAndRequestsPop) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/battle_scene.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = readTextFile(source_path);
    ASSERT_FALSE(source.empty());

    EXPECT_NE(source.find("BattleEndedEvent"), std::string::npos);
    EXPECT_NE(source.find("requestPopScene()"), std::string::npos);
}

} // namespace
} // namespace game::scene
// NOLINTEND
