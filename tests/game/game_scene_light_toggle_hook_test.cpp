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

TEST(GameSceneLightToggleHookTest, UpdatesPlayerFollowLightAfterMapTransition) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/game_scene.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = readTextFile(source_path);
    ASSERT_FALSE(source.empty());

    const std::string map_update_call = "map_transition_system_->update();";
    const std::string light_toggle_update_call = "light_toggle_system_->update();";

    const auto map_update_pos_1 = source.find(map_update_call);
    ASSERT_NE(map_update_pos_1, std::string::npos) << "GameScene::update should call map_transition_system_->update().";

    const auto map_update_pos_2 = source.find(map_update_call, map_update_pos_1 + map_update_call.size());
    ASSERT_NE(map_update_pos_2, std::string::npos) << "Expected at least two map_transition_system_->update() calls.";

    const auto light_toggle_update_pos_1 = source.find(light_toggle_update_call, map_update_pos_1);
    ASSERT_NE(light_toggle_update_pos_1, std::string::npos)
        << "GameScene::update should call light_toggle_system_->update() after map_transition_system_->update() "
           "in the transition branch.";
    EXPECT_LT(light_toggle_update_pos_1, map_update_pos_2);

    const auto light_toggle_update_pos_2 = source.find(light_toggle_update_call, map_update_pos_2);
    ASSERT_NE(light_toggle_update_pos_2, std::string::npos)
        << "GameScene::update should call light_toggle_system_->update() after map_transition_system_->update() "
           "in the normal update branch.";
}

} // namespace
} // namespace game::scene
// NOLINTEND

