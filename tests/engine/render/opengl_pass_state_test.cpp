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

TEST(OpenGLPassStateTest, ClearMethodsUnbindFramebufferAfterClear) {
    const std::filesystem::path scene_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/render/opengl/scene_pass.cpp").lexically_normal();
    const std::filesystem::path lighting_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/render/opengl/lighting_pass.cpp").lexically_normal();
    const std::filesystem::path emissive_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/render/opengl/emissive_pass.cpp").lexically_normal();

    const std::string scene_content = test_source_utils::readTextFile(scene_path);
    const std::string lighting_content = test_source_utils::readTextFile(lighting_path);
    const std::string emissive_content = test_source_utils::readTextFile(emissive_path);

    ASSERT_FALSE(scene_content.empty()) << "无法读取: " << scene_path;
    ASSERT_FALSE(lighting_content.empty()) << "无法读取: " << lighting_path;
    ASSERT_FALSE(emissive_content.empty()) << "无法读取: " << emissive_path;

    const std::string scene_clear = test_source_utils::extractFunctionBlock(scene_content, "void ScenePass::clear(");
    const std::string lighting_clear =
        test_source_utils::extractFunctionBlock(lighting_content, "void LightingPass::clear()");
    const std::string emissive_clear =
        test_source_utils::extractFunctionBlock(emissive_content, "void EmissivePass::clear()");

    ASSERT_FALSE(scene_clear.empty());
    ASSERT_FALSE(lighting_clear.empty());
    ASSERT_FALSE(emissive_clear.empty());

    EXPECT_NE(scene_clear.find("glBindFramebuffer(GL_FRAMEBUFFER, 0);"), std::string::npos);
    EXPECT_NE(lighting_clear.find("glBindFramebuffer(GL_FRAMEBUFFER, 0);"), std::string::npos);
    EXPECT_NE(emissive_clear.find("glBindFramebuffer(GL_FRAMEBUFFER, 0);"), std::string::npos);
}

TEST(OpenGLPassStateTest, BlendSensitivePassesUseScopedBlendGuard) {
    const std::filesystem::path bloom_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/render/opengl/bloom_pass.cpp").lexically_normal();
    const std::filesystem::path lighting_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/render/opengl/lighting_pass.cpp").lexically_normal();
    const std::filesystem::path emissive_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/render/opengl/emissive_pass.cpp").lexically_normal();

    const std::string bloom_content = test_source_utils::readTextFile(bloom_path);
    const std::string lighting_content = test_source_utils::readTextFile(lighting_path);
    const std::string emissive_content = test_source_utils::readTextFile(emissive_path);

    ASSERT_FALSE(bloom_content.empty()) << "无法读取: " << bloom_path;
    ASSERT_FALSE(lighting_content.empty()) << "无法读取: " << lighting_path;
    ASSERT_FALSE(emissive_content.empty()) << "无法读取: " << emissive_path;

    EXPECT_NE(bloom_content.find("const ScopedGLBlendFunc scoped_blend("), std::string::npos);
    EXPECT_NE(lighting_content.find("const ScopedGLBlendFunc scoped_blend("), std::string::npos);
    EXPECT_NE(emissive_content.find("const ScopedGLBlendFunc scoped_blend("), std::string::npos);
}

} // namespace
} // namespace engine::render::opengl
// NOLINTEND
