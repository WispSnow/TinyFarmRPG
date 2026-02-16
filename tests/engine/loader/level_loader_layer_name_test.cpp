// NOLINTBEGIN
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

namespace engine::loader {
namespace {

[[nodiscard]] std::string readTextFile(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return {};
    }
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

TEST(LevelLoaderLayerNameTest, SetsCurrentLayerNameBeforeDispatchingLayerLoaders) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/loader/level_loader.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string content = readTextFile(source_path);
    ASSERT_FALSE(content.empty()) << "无法读取: " << source_path;

    const std::size_t loop_pos = content.find("for (const auto& layer_json : json_data[\"layers\"])");
    ASSERT_NE(loop_pos, std::string::npos) << "未找到 loadLevel 的 layers 循环";

    const std::size_t scan_end = std::min(content.size(), loop_pos + 4000);
    const std::string_view loop_slice(content.data() + loop_pos, scan_end - loop_pos);

    const std::size_t name_pos = loop_slice.find("current_layer_name_ =");
    ASSERT_NE(name_pos, std::string_view::npos) << "未找到 current_layer_name_ 的赋值";

    auto findLoadCall = [&](std::string_view needle) -> std::size_t {
        const std::size_t pos = loop_slice.find(needle);
        return (pos == std::string_view::npos) ? static_cast<std::size_t>(-1) : pos;
    };

    const std::size_t image_pos = findLoadCall("loadImageLayer(layer_json)");
    const std::size_t tile_pos = findLoadCall("loadTileLayer(layer_json)");
    const std::size_t object_pos = findLoadCall("loadObjectLayer(layer_json)");
    const std::size_t first_load = std::min({image_pos, tile_pos, object_pos});

    ASSERT_NE(first_load, static_cast<std::size_t>(-1)) << "未找到 loadImageLayer/loadTileLayer/loadObjectLayer 调用";
    EXPECT_LT(name_pos, first_load)
        << "current_layer_name_ 必须在调用 load*Layer 之前更新，否则 BasicEntityBuilder 会用到上一层名称（例如 solid/collider 误渲染）。";
}

} // namespace
} // namespace engine::loader
// NOLINTEND

