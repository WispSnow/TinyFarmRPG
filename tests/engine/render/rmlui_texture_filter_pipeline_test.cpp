// NOLINTBEGIN
#include <gtest/gtest.h>

#include <filesystem>
#include <string>

#include "test_source_utils.h"

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

namespace engine::ui::rmlui {
namespace {

TEST(RmlUiTextureFilterPipelineTest, LoadTextureUsesVirtualGenerateTextureDispatch) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/ui/rmlui/render_interface_gl3_stb.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string content = test_source_utils::readTextFile(source_path);
    ASSERT_FALSE(content.empty()) << "无法读取: " << source_path;

    const std::string load_texture_block =
        test_source_utils::extractFunctionBlock(content, "Rml::TextureHandle RenderInterface_GL3_STB::LoadTexture(");
    ASSERT_FALSE(load_texture_block.empty());

    EXPECT_EQ(load_texture_block.find("RenderInterface_GL3::GenerateTexture("), std::string::npos);
    EXPECT_NE(load_texture_block.find("this->GenerateTexture("), std::string::npos);
}

TEST(RmlUiTextureFilterPipelineTest, GenerateTextureTracksHandlesAndAppliesConfiguredFilter) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/ui/rmlui/render_interface_gl3_stb.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string content = test_source_utils::readTextFile(source_path);
    ASSERT_FALSE(content.empty()) << "无法读取: " << source_path;

    const std::string generate_texture_block =
        test_source_utils::extractFunctionBlock(content, "Rml::TextureHandle RenderInterface_GL3_STB::GenerateTexture(");
    ASSERT_FALSE(generate_texture_block.empty());

    EXPECT_NE(generate_texture_block.find("RenderInterface_GL3::GenerateTexture(source_data, source_dimensions)"), std::string::npos);
    EXPECT_NE(generate_texture_block.find("tracked_texture_handles_.insert(texture_handle);"), std::string::npos);
    EXPECT_NE(generate_texture_block.find("applyTextureFilter(texture_handle);"), std::string::npos);

    const std::string set_mode_block =
        test_source_utils::extractFunctionBlock(content, "void RenderInterface_GL3_STB::setTextureFilterMode(");
    ASSERT_FALSE(set_mode_block.empty());
    EXPECT_NE(set_mode_block.find("for (const Rml::TextureHandle texture_handle : tracked_texture_handles_)"), std::string::npos);

    EXPECT_NE(content.find("glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter);"), std::string::npos);
    EXPECT_NE(content.find("glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter);"), std::string::npos);
    EXPECT_NE(content.find("GL_NEAREST"), std::string::npos);
    EXPECT_NE(content.find("GL_LINEAR"), std::string::npos);
}

TEST(RmlUiTextureFilterPipelineTest, SaveLayerAsTextureStillUsesDedicatedEffectPath) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "external/RmlUi-6.2/Backends/RmlUi_Renderer_GL3.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string content = test_source_utils::readTextFile(source_path);
    ASSERT_FALSE(content.empty()) << "无法读取: " << source_path;

    const std::string save_layer_block =
        test_source_utils::extractFunctionBlock(content, "Rml::TextureHandle RenderInterface_GL3::SaveLayerAsTexture()");
    ASSERT_FALSE(save_layer_block.empty());

    EXPECT_NE(save_layer_block.find("Gfx::CreateTexture({}, bounds.Size());"), std::string::npos);
    EXPECT_EQ(save_layer_block.find("GenerateTexture("), std::string::npos);
}

} // namespace
} // namespace engine::ui::rmlui

namespace engine::render::opengl {
namespace {

TEST(RmlUiTextureFilterPipelineTest, GlRendererCachesAndForwardsTextureFilterMode) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/render/opengl/gl_renderer.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string content = test_source_utils::readTextFile(source_path);
    ASSERT_FALSE(content.empty()) << "无法读取: " << source_path;

    const std::string set_filter_block =
        test_source_utils::extractFunctionBlock(content, "void GLRenderer::setRmlUiTextureFilterMode(");
    ASSERT_FALSE(set_filter_block.empty());
    EXPECT_NE(set_filter_block.find("rmlui_texture_filter_mode_ = mode;"), std::string::npos);
    EXPECT_NE(set_filter_block.find("rmlui_legacy_layer_->setTextureFilterMode(mode);"), std::string::npos);

    const std::string set_legacy_layer_block =
        test_source_utils::extractFunctionBlock(content, "void GLRenderer::setLegacyRmlUiLayer(");
    ASSERT_FALSE(set_legacy_layer_block.empty());
    EXPECT_NE(set_legacy_layer_block.find("rmlui_legacy_layer_->setTextureFilterMode(rmlui_texture_filter_mode_);"),
              std::string::npos);
}

TEST(RmlUiTextureFilterPipelineTest, RmlUiLayerForwardsTextureFilterToRenderBackend) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/ui/rmlui/rml_ui_layer.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string content = test_source_utils::readTextFile(source_path);
    ASSERT_FALSE(content.empty()) << "无法读取: " << source_path;

    const std::string set_filter_block =
        test_source_utils::extractFunctionBlock(content, "void RmlUILayer::setTextureFilterMode(");
    ASSERT_FALSE(set_filter_block.empty());
    EXPECT_NE(set_filter_block.find("render_backend_->setTextureFilterMode(mode);"), std::string::npos);
}

TEST(RmlUiTextureFilterPipelineTest, RmlUiRenderBackendForwardsTextureFilterToRenderInterface) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/ui/rmlui/rml_ui_render_backend_gl.cpp")
            .lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string content = test_source_utils::readTextFile(source_path);
    ASSERT_FALSE(content.empty()) << "无法读取: " << source_path;

    const std::string set_filter_block =
        test_source_utils::extractFunctionBlock(content, "void RmlUiRenderBackendGl::setTextureFilterMode(");
    ASSERT_FALSE(set_filter_block.empty());
    EXPECT_NE(set_filter_block.find("render_interface_->setTextureFilterMode(mode);"), std::string::npos);
}

TEST(RmlUiTextureFilterPipelineTest, RmlUiDebugPanelExposesRuntimeToggle) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/debug/panels/rmlui_debug_panel.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string content = test_source_utils::readTextFile(source_path);
    ASSERT_FALSE(content.empty()) << "无法读取: " << source_path;

    EXPECT_NE(content.find("Nearest (Pixel Art)"), std::string::npos);
    EXPECT_NE(content.find("Linear (Smooth)"), std::string::npos);
    EXPECT_NE(content.find("render_backend_.setTextureFilterMode("), std::string::npos);
    EXPECT_NE(content.find("context_.getRmlUi()"), std::string::npos);
}

} // namespace
} // namespace engine::render::opengl

namespace engine::core {
namespace {

TEST(RmlUiTextureFilterPipelineTest, GameAppAppliesConfiguredFilterAfterRendererCreation) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/core/game_app.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string content = test_source_utils::readTextFile(source_path);
    ASSERT_FALSE(content.empty()) << "无法读取: " << source_path;

    const std::string init_gl_renderer_block =
        test_source_utils::extractFunctionBlock(content, "bool GameApp::initGLRenderer()");
    ASSERT_FALSE(init_gl_renderer_block.empty());
    EXPECT_NE(init_gl_renderer_block.find("gl_renderer_->setRmlUiTextureFilterMode(config_->rmlui_texture_filter_mode_);"),
              std::string::npos);
}

} // namespace
} // namespace engine::core
// NOLINTEND
