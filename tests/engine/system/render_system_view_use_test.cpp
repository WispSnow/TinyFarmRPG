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

TEST(RenderSystemViewUseTest, ForcesViewToIterateSortedRenderComponentStorage) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/system/render_system.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string content = readTextFile(source_path);
    ASSERT_FALSE(content.empty()) << "无法读取: " << source_path;

    const std::size_t view_pos = content.find("auto view = registry.view<component::RenderComponent");
    ASSERT_NE(view_pos, std::string::npos) << "未找到 RenderSystem::update 的 view 创建语句";

    const std::size_t scan_end = std::min(content.size(), view_pos + 1200);
    const std::string_view view_slice(content.data() + view_pos, scan_end - view_pos);

    const std::size_t use_pos = view_slice.find("view.use<component::RenderComponent>()");
    ASSERT_NE(use_pos, std::string_view::npos)
        << "RenderSystem 必须调用 view.use<RenderComponent>()，否则 EnTT 会自动选择最小 storage 导致排序无效。";
}

} // namespace
} // namespace engine::system
// NOLINTEND

