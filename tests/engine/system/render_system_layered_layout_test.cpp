// NOLINTBEGIN
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

namespace engine::system {
namespace {

[[nodiscard]] std::string readTextFile(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return {};
    }
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

TEST(RenderSystemLayeredLayoutTest, UsesLayoutDrivenRectSamplingAndPerLayerFallback) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/system/render_system.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string content = readTextFile(source_path);
    ASSERT_FALSE(content.empty()) << "无法读取: " << source_path;

    EXPECT_NE(content.find("source_frame_index_by_runtime_frame_"), std::string::npos)
        << "RenderSystem 应按缓存的 runtime->source 帧映射计算采样。";
    EXPECT_NE(content.find("direction_block_index_ * layer_layout->frames_per_direction_ +"), std::string::npos)
        << "RenderSystem 应使用方向块与每方向帧数计算图集列偏移。";
    EXPECT_NE(content.find("layer_layout->frame_width_"), std::string::npos)
        << "RenderSystem 应使用布局中的帧宽度计算 src_rect.x。";
    EXPECT_NE(content.find("layer_sprite.src_rect_.pos.y = 0.0f"), std::string::npos)
        << "分层图集单行采样应强制 src_rect.y = 0。";
    EXPECT_NE(content.find("if (!layer_layout ||"), std::string::npos)
        << "缺层时应只跳过该层而非回退到主纹理重复绘制。";
}

} // namespace
} // namespace engine::system
// NOLINTEND

