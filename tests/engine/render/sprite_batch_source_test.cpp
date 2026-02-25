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

TEST(SpriteBatchSourceTest, VertexCountTracksVerticesNotIndices) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/render/opengl/sprite_batch.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string content = test_source_utils::readTextFile(source_path);
    ASSERT_FALSE(content.empty()) << "无法读取: " << source_path;

    EXPECT_EQ(content.find("vertex_count += cmd.index_count_"), std::string::npos)
        << "SpriteBatch::flush 应该统计 vertex 数（每精灵 4），不应把 indices（每精灵 6）累加到 vertex_count。";
}

TEST(SpriteBatchSourceTest, EmptyRectDoesNotLogError) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/render/opengl/sprite_batch.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string content = test_source_utils::readTextFile(source_path);
    ASSERT_FALSE(content.empty()) << "无法读取: " << source_path;

    EXPECT_EQ(content.find("SpriteBatch::queueSprite: rect is empty"), std::string::npos)
        << "空 rect 绘制应被静默忽略或降级日志，避免在运行期刷屏 error。";
}

} // namespace
} // namespace engine::render::opengl
// NOLINTEND
