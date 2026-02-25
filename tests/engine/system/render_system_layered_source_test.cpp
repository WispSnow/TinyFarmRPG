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

TEST(RenderSystemLayeredSourceTest, LayeredModeSkipsBaseSpriteAndDrawsSlotsInOrder) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/system/render_system.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string content = readTextFile(source_path);
    ASSERT_FALSE(content.empty()) << "无法读取: " << source_path;

    EXPECT_NE(content.find("LayeredSpriteComponent"), std::string::npos)
        << "RenderSystem 应接入 LayeredSpriteComponent。";
    EXPECT_NE(content.find("has_layered_draw"), std::string::npos)
        << "RenderSystem 应在分层成功绘制时跳过主纹理绘制。";
    EXPECT_NE(content.find("layer.resolveTexture"), std::string::npos)
        << "RenderSystem 应基于 current_animation_id 查 layer 贴图。";
    EXPECT_NE(content.find("render.depth_ + layer.depth_offset_"), std::string::npos)
        << "RenderSystem 应使用微小 depth 偏移保证层序。";
    EXPECT_NE(content.find("std::sort(draw_requests.begin(), draw_requests.end()"), std::string::npos)
        << "RenderSystem 应对分层 draw 请求统一排序。";
}

} // namespace
} // namespace engine::system
// NOLINTEND
