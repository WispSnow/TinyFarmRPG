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

TEST(VfxDualChannelPipelineTest, ClearIncludesWorldVfxPassClear) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/render/opengl/gl_renderer.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string content = test_source_utils::readTextFile(source_path);
    ASSERT_FALSE(content.empty()) << "无法读取: " << source_path;

    const std::string clear_block = test_source_utils::extractFunctionBlock(content, "void GLRenderer::clear()");
    ASSERT_FALSE(clear_block.empty());
    EXPECT_NE(clear_block.find("world_vfx_pass_->clear();"), std::string::npos);
}

TEST(VfxDualChannelPipelineTest, ClearResetsDefaultDepthForOverlayVfx) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/render/opengl/gl_renderer.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string content = test_source_utils::readTextFile(source_path);
    ASSERT_FALSE(content.empty()) << "无法读取: " << source_path;

    const std::string clear_block = test_source_utils::extractFunctionBlock(content, "void GLRenderer::clear()");
    ASSERT_FALSE(clear_block.empty());
    EXPECT_NE(clear_block.find("glClearDepth(1.0);"), std::string::npos);
    EXPECT_NE(clear_block.find("glDepthMask(GL_TRUE);"), std::string::npos);
    EXPECT_NE(clear_block.find("GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT"), std::string::npos);
}

TEST(VfxDualChannelPipelineTest, CompositePassBindsWorldVfxTextureUniform) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/render/opengl/composite_pass.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string content = test_source_utils::readTextFile(source_path);
    ASSERT_FALSE(content.empty()) << "无法读取: " << source_path;

    EXPECT_NE(content.find("u_world_vfx_tex_"), std::string::npos);
    EXPECT_NE(content.find("glActiveTexture(GL_TEXTURE4);"), std::string::npos);
}

} // namespace
} // namespace engine::render::opengl
// NOLINTEND
