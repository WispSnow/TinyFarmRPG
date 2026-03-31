// NOLINTBEGIN
#include <gtest/gtest.h>

#include <filesystem>
#include <string>

#include "../render/test_source_utils.h"

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

namespace engine::core {
namespace {

TEST(GameAppRmlUiFrameLoopTest, RenderPreparesUiAndUpdatesRuntimeBeforeSceneRender) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/core/game_app.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = test_source_utils::readTextFile(source_path);
    ASSERT_FALSE(source.empty()) << "无法读取: " << source_path;

    const std::string render_block = test_source_utils::extractFunctionBlock(source, "void GameApp::render(float interpolation_alpha)");
    ASSERT_FALSE(render_block.empty());

    const std::size_t pos_prepare_ui = render_block.find("scene_manager_->prepareUi(interpolation_alpha);");
    const std::size_t pos_update_rmlui = render_block.find("updateRmlUiFrame();");
    const std::size_t pos_scene_render = render_block.find("scene_manager_->render(interpolation_alpha);");
    const std::size_t pos_present = render_block.find("renderer_->present();");

    ASSERT_NE(pos_prepare_ui, std::string::npos);
    ASSERT_NE(pos_update_rmlui, std::string::npos);
    ASSERT_NE(pos_scene_render, std::string::npos);
    ASSERT_NE(pos_present, std::string::npos);

    EXPECT_LT(pos_prepare_ui, pos_update_rmlui);
    EXPECT_LT(pos_update_rmlui, pos_scene_render);
    EXPECT_LT(pos_scene_render, pos_present);
}

TEST(GameAppRmlUiFrameLoopTest, UpdateRmlUiFrameOwnsRuntimeUpdate) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/core/game_app.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = test_source_utils::readTextFile(source_path);
    ASSERT_FALSE(source.empty()) << "无法读取: " << source_path;

    const std::string update_block = test_source_utils::extractFunctionBlock(source, "void GameApp::updateRmlUiFrame()");
    ASSERT_FALSE(update_block.empty());

    EXPECT_NE(update_block.find("if (rmlui_runtime_)"), std::string::npos);
    EXPECT_NE(update_block.find("rmlui_runtime_->update();"), std::string::npos);
}

TEST(GameAppRmlUiFrameLoopTest, InitRmlUiRegistersRenderOnlyHook) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/core/game_app.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = test_source_utils::readTextFile(source_path);
    ASSERT_FALSE(source.empty()) << "无法读取: " << source_path;

    const std::string init_block = test_source_utils::extractFunctionBlock(source, "bool GameApp::initRmlUi()");
    ASSERT_FALSE(init_block.empty());

    EXPECT_NE(init_block.find("gl_renderer_->setRmlUiRenderHook([this](const engine::utils::Rect& current_viewport) {"),
              std::string::npos);
    EXPECT_NE(init_block.find("rmlui_render_backend_->render(*context, toRmlUiViewport(current_viewport));"),
              std::string::npos);
    EXPECT_EQ(init_block.find("rmlui_runtime_->update();"), std::string::npos)
        << "RmlUi runtime update should no longer live inside the render hook.";
}

} // namespace
} // namespace engine::core
// NOLINTEND
