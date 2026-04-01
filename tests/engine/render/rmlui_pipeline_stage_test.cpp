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

TEST(RmlUiPipelineStageTest, PresentCallsRmlUiRenderHookBetweenOverlayVfxAndImGui) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/render/opengl/gl_renderer.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string content = test_source_utils::readTextFile(source_path);
    ASSERT_FALSE(content.empty()) << "无法读取: " << source_path;

    const std::string present_block = test_source_utils::extractFunctionBlock(content, "void GLRenderer::present()");
    ASSERT_FALSE(present_block.empty());

    const std::size_t pos_overlay_vfx = present_block.find("vfx_pass_->flush(overlay_vfx_context)");
    const std::size_t pos_rml_hook = present_block.find("rmlui_render_hook_(viewport);");
    const std::size_t pos_srgb_restore = present_block.find("glEnable(GL_FRAMEBUFFER_SRGB);");
    const std::size_t pos_imgui = present_block.find("imgui_layer_->endFrame();");

    ASSERT_NE(pos_overlay_vfx, std::string::npos);
    ASSERT_NE(pos_rml_hook, std::string::npos);
    ASSERT_NE(pos_srgb_restore, std::string::npos);
    ASSERT_NE(pos_imgui, std::string::npos);

    EXPECT_LT(pos_overlay_vfx, pos_rml_hook);
    EXPECT_LT(pos_rml_hook, pos_srgb_restore);
    EXPECT_LT(pos_srgb_restore, pos_imgui);
}

TEST(RmlUiPipelineStageTest, CleanClearsRmlUiHookBeforeRenderContext) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/render/opengl/gl_renderer.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string content = test_source_utils::readTextFile(source_path);
    ASSERT_FALSE(content.empty()) << "无法读取: " << source_path;

    const std::string clean_block = test_source_utils::extractFunctionBlock(content, "void GLRenderer::clean()");
    ASSERT_FALSE(clean_block.empty());

    const std::size_t pos_hook_reset = clean_block.find("rmlui_render_hook_ = {};");
    const std::size_t pos_context_clean = clean_block.find("render_context_->clean();");

    ASSERT_NE(pos_hook_reset, std::string::npos);
    ASSERT_NE(pos_context_clean, std::string::npos);

    EXPECT_LT(pos_hook_reset, pos_context_clean);
}

} // namespace
} // namespace engine::render::opengl
// NOLINTEND
