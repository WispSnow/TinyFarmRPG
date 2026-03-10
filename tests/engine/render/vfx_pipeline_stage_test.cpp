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

TEST(VfxPipelineStageTest, PassTypeInsertsWorldAndOverlayVfxAfterBloom) {
    const std::filesystem::path header_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/render/opengl/gl_renderer.h").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(header_path)) << header_path;

    const std::string content = test_source_utils::readTextFile(header_path);
    ASSERT_FALSE(content.empty()) << "无法读取: " << header_path;

    const std::size_t pos_bloom = content.find("Bloom,");
    const std::size_t pos_world_vfx = content.find("WorldVfx,");
    const std::size_t pos_overlay_vfx = content.find("OverlayVfx,");
    const std::size_t pos_count = content.find("Count");

    ASSERT_NE(pos_bloom, std::string::npos);
    ASSERT_NE(pos_world_vfx, std::string::npos);
    ASSERT_NE(pos_overlay_vfx, std::string::npos);
    ASSERT_NE(pos_count, std::string::npos);
    EXPECT_LT(pos_bloom, pos_world_vfx);
    EXPECT_LT(pos_world_vfx, pos_overlay_vfx);
    EXPECT_LT(pos_overlay_vfx, pos_count);
}

TEST(VfxPipelineStageTest, PresentRunsWorldVfxBeforeCompositeAndOverlayBeforeRmlUi) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/render/opengl/gl_renderer.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string content = test_source_utils::readTextFile(source_path);
    ASSERT_FALSE(content.empty()) << "无法读取: " << source_path;

    const std::string present_block = test_source_utils::extractFunctionBlock(content, "void GLRenderer::present()");
    ASSERT_FALSE(present_block.empty());

    const std::size_t pos_world_vfx = present_block.find("world_vfx_pass_->flush(world_vfx_context)");
    const std::size_t pos_composite = present_block.find("composite_pass_->render(viewport)");
    const std::size_t pos_overlay_vfx = present_block.find("vfx_pass_->flush(overlay_vfx_context)");
    const std::size_t pos_rml_update = present_block.find("rmlui_layer_->update();");

    ASSERT_NE(pos_world_vfx, std::string::npos);
    ASSERT_NE(pos_composite, std::string::npos);
    ASSERT_NE(pos_overlay_vfx, std::string::npos);
    ASSERT_NE(pos_rml_update, std::string::npos);
    EXPECT_LT(pos_world_vfx, pos_composite);
    EXPECT_LT(pos_composite, pos_overlay_vfx);
    EXPECT_LT(pos_overlay_vfx, pos_rml_update);
}

TEST(VfxPipelineStageTest, CleanReleasesWorldAndOverlayVfxPasses) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/render/opengl/gl_renderer.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string content = test_source_utils::readTextFile(source_path);
    ASSERT_FALSE(content.empty()) << "无法读取: " << source_path;

    const std::string clean_block = test_source_utils::extractFunctionBlock(content, "void GLRenderer::clean()");
    ASSERT_FALSE(clean_block.empty());
    EXPECT_NE(clean_block.find("world_vfx_pass_->clean();"), std::string::npos);
    EXPECT_NE(clean_block.find("world_vfx_pass_.reset();"), std::string::npos);
    EXPECT_NE(clean_block.find("vfx_pass_->clean();"), std::string::npos);
    EXPECT_NE(clean_block.find("vfx_pass_.reset();"), std::string::npos);
}

TEST(VfxPipelineStageTest, SetVfxBackendDelegatesToWorldAndOverlayPasses) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/render/opengl/gl_renderer.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string content = test_source_utils::readTextFile(source_path);
    ASSERT_FALSE(content.empty()) << "无法读取: " << source_path;

    const std::string function_block =
        test_source_utils::extractFunctionBlock(content, "void GLRenderer::setVfxBackend(");
    ASSERT_FALSE(function_block.empty());

    EXPECT_NE(function_block.find("world_vfx_pass_->setBackend(backend);"), std::string::npos);
    EXPECT_NE(function_block.find("vfx_pass_->setBackend(backend);"), std::string::npos);
}

} // namespace
} // namespace engine::render::opengl
// NOLINTEND
