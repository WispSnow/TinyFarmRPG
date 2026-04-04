// NOLINTBEGIN
#include <gtest/gtest.h>

#include <filesystem>
#include <string>

#include "../render/test_source_utils.h"

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

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

    const std::string scene_header = test_source_utils::readTextFile(scene_header_path);
    const std::string scene_source = test_source_utils::readTextFile(scene_source_path);
    const std::string manager_source = test_source_utils::readTextFile(manager_source_path);

    ASSERT_FALSE(scene_header.empty());
    ASSERT_FALSE(scene_source.empty());
    ASSERT_FALSE(manager_source.empty());

    EXPECT_NE(scene_header.find("virtual void render(float interpolation_alpha)"), std::string::npos)
        << "Scene base class should expose render(alpha) signature.";
    EXPECT_NE(scene_header.find("virtual void prepareUi(float interpolation_alpha)"), std::string::npos)
        << "Scene base class should expose a prepareUi(alpha) hook for retained UI composition.";
    EXPECT_NE(scene_source.find("void Scene::render(float"), std::string::npos)
        << "Scene default render implementation should accept alpha argument.";
    EXPECT_NE(scene_source.find("void Scene::prepareUi(float"), std::string::npos)
        << "Scene default prepareUi implementation should accept alpha argument.";
    EXPECT_NE(manager_source.find("void SceneManager::prepareUi(float interpolation_alpha)"), std::string::npos)
        << "SceneManager should expose prepareUi(alpha) entry.";
    EXPECT_NE(manager_source.find("scene->prepareUi(interpolation_alpha);"), std::string::npos)
        << "SceneManager should forward alpha to the top scene prepareUi path.";
    EXPECT_NE(manager_source.find("void SceneManager::render(float interpolation_alpha)"), std::string::npos)
        << "SceneManager should expose render(alpha) entry.";
    EXPECT_NE(manager_source.find("scene->render(interpolation_alpha);"), std::string::npos)
        << "SceneManager should forward alpha to all stacked scenes.";
}

TEST(RenderInterpolationPipelineTest, GameSceneConsumesAlphaForRenderSystems) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/scene/game_scene.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = test_source_utils::readTextFile(source_path);
    ASSERT_FALSE(source.empty());

    EXPECT_NE(source.find("void GameScene::render(float interpolation_alpha)"), std::string::npos)
        << "GameScene render signature should accept alpha.";
    EXPECT_NE(source.find("void GameScene::prepareUi(float interpolation_alpha)"), std::string::npos)
        << "GameScene should prepare retained UI composition before RmlUi update.";
    EXPECT_NE(source.find("const float clamped_alpha = std::clamp(interpolation_alpha, 0.0f, 1.0f);"), std::string::npos)
        << "GameScene render should clamp incoming alpha before use.";
    EXPECT_NE(source.find("ui_controller_->refreshAnchoredWidgets(camera, clamped_alpha);"), std::string::npos)
        << "UI controller should refresh anchored widgets (dialogue bubbles) in the prepareUi path.";
    EXPECT_NE(source.find("systems_->ysort_system->render(registry_, clamped_alpha);"), std::string::npos)
        << "YSort should consume interpolated alpha path.";
    EXPECT_NE(source.find("systems_->render_system->render(registry_, renderer, camera, clamped_alpha);"), std::string::npos)
        << "RenderSystem should consume interpolated alpha path.";
    EXPECT_NE(source.find("systems_->light_system->render(registry_, renderer, clamped_alpha);"), std::string::npos)
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
