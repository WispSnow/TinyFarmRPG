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

TEST(GLRendererLifecycleTest, CleanOrdersShaderLibraryBeforeRenderContextCleanup) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/render/opengl/gl_renderer.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string content = test_source_utils::readTextFile(source_path);
    ASSERT_FALSE(content.empty()) << "无法读取: " << source_path;

    const std::size_t pos_scene_reset = content.find("scene_pass_.reset();");
    const std::size_t pos_shader_clear = content.find("shader_library_->clear();");
    const std::size_t pos_shader_reset = content.find("shader_library_.reset();");
    const std::size_t pos_context_clean = content.find("render_context_->clean();");
    const std::size_t pos_context_reset = content.find("render_context_.reset();");

    ASSERT_NE(pos_scene_reset, std::string::npos);
    ASSERT_NE(pos_shader_clear, std::string::npos);
    ASSERT_NE(pos_shader_reset, std::string::npos);
    ASSERT_NE(pos_context_clean, std::string::npos);
    ASSERT_NE(pos_context_reset, std::string::npos);

    EXPECT_LT(pos_scene_reset, pos_shader_clear);
    EXPECT_LT(pos_shader_clear, pos_context_clean);
    EXPECT_LT(pos_shader_reset, pos_context_clean);
    EXPECT_LT(pos_context_clean, pos_context_reset);
}

TEST(GLRendererLifecycleTest, LightApisGuardWhenLightingPassUnavailable) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/render/opengl/gl_renderer.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string content = test_source_utils::readTextFile(source_path);
    ASSERT_FALSE(content.empty()) << "无法读取: " << source_path;

    EXPECT_NE(content.find("if (!point_lights_enabled_ || !lighting_pass_) return;"), std::string::npos);
    EXPECT_NE(content.find("if (!spot_lights_enabled_ || !lighting_pass_) return;"), std::string::npos);
    EXPECT_NE(content.find("if (!directional_lights_enabled_ || !lighting_pass_) return;"), std::string::npos);
}

} // namespace
} // namespace engine::render::opengl
// NOLINTEND
