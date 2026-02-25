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

    const std::size_t rgb16f_uploads =
        test_source_utils::countOccurrences(content, "glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F");
    EXPECT_GE(rgb16f_uploads, static_cast<std::size_t>(2))
        << "Bloom ping-pong 至少需要两次 GL_RGB16F 纹理分配（ping/pong）。";
    EXPECT_EQ(content.find("GL_RGB8"), std::string::npos)
        << "Bloom ping-pong 不应退回到 LDR 的 GL_RGB8。";
    EXPECT_NE(content.find("GL_FLOAT"), std::string::npos)
        << "Bloom HDR 纹理上传应保持浮点类型。";
}

} // namespace
} // namespace engine::render::opengl
// NOLINTEND
