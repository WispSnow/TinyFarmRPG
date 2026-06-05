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

TEST(BloomPrecisionRegressionTest, PingPongTexturesUseHdrFormat) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/render/opengl/bloom_pass.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string content = test_source_utils::readTextFile(source_path);
    ASSERT_FALSE(content.empty()) << "无法读取: " << source_path;

    const std::size_t rgba16f_uploads =
        test_source_utils::countOccurrences(content, "glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F");
    EXPECT_GE(rgba16f_uploads, static_cast<std::size_t>(2))
        << "Bloom ping-pong 至少需要两次 GL_RGBA16F 纹理分配（ping/pong）。";
    EXPECT_EQ(content.find("GL_RGB16F"), std::string::npos)
        << "WebGL2 下 GL_RGB16F 不是可靠的 color-renderable Bloom 目标。";
    EXPECT_EQ(content.find("GL_RGB8"), std::string::npos)
        << "Bloom ping-pong 不应退回到 LDR 的 GL_RGB8。";
    EXPECT_NE(content.find("GL_HALF_FLOAT"), std::string::npos)
        << "Bloom HDR 纹理上传应使用 half-float 数据类型。";
}

} // namespace
} // namespace engine::render::opengl
// NOLINTEND
