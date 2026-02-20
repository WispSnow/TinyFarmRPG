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

TEST(RenderInterpolationPipelineTest, SceneAndManagerExposeAlphaRenderPath) {
    const std::filesystem::path scene_header_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/scene/scene.h").lexically_normal();
    const std::filesystem::path scene_source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/scene/scene.cpp").lexically_normal();
    const std::filesystem::path manager_source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/scene/scene_manager.cpp").lexically_normal();

    ASSERT_TRUE(std::filesystem::exists(scene_header_path)) << scene_header_path;
    ASSERT_TRUE(std::filesystem::exists(scene_source_path)) << scene_source_path;
    ASSERT_TRUE(std::filesystem::exists(manager_source_path)) << manager_source_path;

    const std::string scene_header = readTextFile(scene_header_path);
    const std::string scene_source = readTextFile(scene_source_path);
    const std::string manager_source = readTextFile(manager_source_path);

    ASSERT_FALSE(scene_header.empty());
    ASSERT_FALSE(scene_source.empty());
    ASSERT_FALSE(manager_source.empty());

    EXPECT_NE(scene_header.find("virtual void render(float interpolation_alpha)"), std::string::npos)
        << "Scene base class should expose render(alpha) signature.";
    EXPECT_NE(scene_source.find("void Scene::render(float"), std::string::npos)
        << "Scene default render implementation should accept alpha argument.";
    EXPECT_NE(manager_source.find("void SceneManager::render(float interpolation_alpha)"), std::string::npos)
        << "SceneManager should expose render(alpha) entry.";
    EXPECT_NE(manager_source.find("scene->render(interpolation_alpha);"), std::string::npos)
        << "SceneManager should forward alpha to all stacked scenes.";
}

TEST(RenderInterpolationPipelineTest, GameSceneConsumesAlphaForRenderSystems) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/game_scene.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = readTextFile(source_path);
    ASSERT_FALSE(source.empty());

    EXPECT_NE(source.find("void GameScene::render(float interpolation_alpha)"), std::string::npos)
        << "GameScene render signature should accept alpha.";
    EXPECT_NE(source.find("const float clamped_alpha = std::clamp(interpolation_alpha, 0.0f, 1.0f);"), std::string::npos)
        << "GameScene render should clamp incoming alpha before use.";
    EXPECT_NE(source.find("systems_->ysort_system->update(registry_, clamped_alpha);"), std::string::npos)
        << "YSort should consume interpolated alpha path.";
    EXPECT_NE(source.find("systems_->render_system->update(registry_, renderer, camera, clamped_alpha);"), std::string::npos)
        << "RenderSystem should consume interpolated alpha path.";
    EXPECT_NE(source.find("systems_->light_system->update(registry_, renderer, clamped_alpha);"), std::string::npos)
        << "LightSystem should consume interpolated alpha path.";
    EXPECT_NE(source.find("Scene::render(interpolation_alpha);"), std::string::npos)
        << "GameScene should forward alpha to Scene base render for UI stack.";

    const auto snapshot_pos = source.find("snapshotInterpolationState();");
    const auto scheduler_pos = source.find("scheduler_->tick({");
    ASSERT_NE(snapshot_pos, std::string::npos);
    ASSERT_NE(scheduler_pos, std::string::npos);
    EXPECT_LT(snapshot_pos, scheduler_pos)
        << "GameScene should snapshot interpolation state before fixed-step gameplay tick.";
}

} // namespace
} // namespace engine::scene
// NOLINTEND
