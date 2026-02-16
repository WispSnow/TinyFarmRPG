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

namespace engine::scene {
namespace {

TEST(EntryToFirstFrameSafetyTest, SceneUpdateGuardsNullUiManager) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/scene/scene.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = readTextFile(source_path);
    ASSERT_FALSE(source.empty());

    EXPECT_NE(source.find("if (ui_manager_)"), std::string::npos)
        << "Scene::update/render should guard ui_manager_ being null.";
}

TEST(EntryToFirstFrameSafetyTest, ReplaceSceneDoesNotAssumeNonEmptyStack) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/scene/scene_manager.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = readTextFile(source_path);
    ASSERT_FALSE(source.empty());

    EXPECT_EQ(source.find(R"(spdlog::debug("正在用场景 '{}' 替换场景 '{}' 。", scene->getName(), scene_stack_.back()->getName());)"),
              std::string::npos)
        << "replaceScene should not dereference scene_stack_.back() without guarding empty stack.";
}

} // namespace
} // namespace engine::scene

namespace engine::core {
namespace {

TEST(EntryToFirstFrameSafetyTest, GameAppHooksQuitAndResizeBeforeSceneSetup) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/core/game_app.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = readTextFile(source_path);
    ASSERT_FALSE(source.empty());

    const auto setup_pos = source.find("scene_setup_func_(*context_)");
    const auto quit_hook_pos = source.find("sink<utils::QuitEvent>().connect<&GameApp::onQuitEvent>");
    const auto resize_hook_pos = source.find("sink<utils::WindowResizedEvent>().connect<&GameApp::onWindowResized>");

    ASSERT_NE(setup_pos, std::string::npos);
    ASSERT_NE(quit_hook_pos, std::string::npos);
    ASSERT_NE(resize_hook_pos, std::string::npos);

    EXPECT_LT(quit_hook_pos, setup_pos);
    EXPECT_LT(resize_hook_pos, setup_pos);
}

} // namespace
} // namespace engine::core
// NOLINTEND

