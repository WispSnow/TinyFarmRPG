// NOLINTBEGIN
#include <gtest/gtest.h>

#include <filesystem>
#include <string>

#include "test_source_utils.h"

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

namespace engine::render::opengl {
namespace {

TEST(RmlUiPipelineStageTest, PresentRendersRmlUiBetweenUiPassAndImGui) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/render/opengl/gl_renderer.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string content = test_source_utils::readTextFile(source_path);
    ASSERT_FALSE(content.empty()) << "无法读取: " << source_path;

    const std::string present_block = test_source_utils::extractFunctionBlock(content, "void GLRenderer::present()");
    ASSERT_FALSE(present_block.empty());

    const std::size_t pos_ui_pass = present_block.find("ui_pass_->flush(viewport)");
    const std::size_t pos_rml_update = present_block.find("rmlui_layer_->update();");
    const std::size_t pos_rml_render = present_block.find("rmlui_layer_->render();");
    const std::size_t pos_srgb_restore = present_block.find("glEnable(GL_FRAMEBUFFER_SRGB);");
    const std::size_t pos_imgui = present_block.find("imgui_layer_->endFrame();");

    ASSERT_NE(pos_ui_pass, std::string::npos);
    ASSERT_NE(pos_rml_update, std::string::npos);
    ASSERT_NE(pos_rml_render, std::string::npos);
    ASSERT_NE(pos_srgb_restore, std::string::npos);
    ASSERT_NE(pos_imgui, std::string::npos);

    EXPECT_LT(pos_ui_pass, pos_rml_update);
    EXPECT_LT(pos_rml_update, pos_rml_render);
    EXPECT_LT(pos_rml_render, pos_srgb_restore);
    EXPECT_LT(pos_srgb_restore, pos_imgui);
}

TEST(RmlUiPipelineStageTest, CleanReleasesRmlUiLayerBeforeRenderContext) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/render/opengl/gl_renderer.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string content = test_source_utils::readTextFile(source_path);
    ASSERT_FALSE(content.empty()) << "无法读取: " << source_path;

    const std::string clean_block = test_source_utils::extractFunctionBlock(content, "void GLRenderer::clean()");
    ASSERT_FALSE(clean_block.empty());

    const std::size_t pos_rml_clean = clean_block.find("rmlui_layer_->clean();");
    const std::size_t pos_rml_reset = clean_block.find("rmlui_layer_.reset();");
    const std::size_t pos_context_clean = clean_block.find("render_context_->clean();");

    ASSERT_NE(pos_rml_clean, std::string::npos);
    ASSERT_NE(pos_rml_reset, std::string::npos);
    ASSERT_NE(pos_context_clean, std::string::npos);

    EXPECT_LT(pos_rml_clean, pos_context_clean);
    EXPECT_LT(pos_rml_reset, pos_context_clean);
}

} // namespace
} // namespace engine::render::opengl
// NOLINTEND
