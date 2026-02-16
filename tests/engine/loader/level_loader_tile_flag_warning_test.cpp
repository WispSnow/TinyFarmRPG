// NOLINTBEGIN
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <regex>
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

TEST(LevelLoaderTileFlagWarningTest, LevelLoaderWarnsOnUnknownTileFlagTokens) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/loader/level_loader.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string content = readTextFile(source_path);
    ASSERT_FALSE(content.empty()) << "无法读取: " << source_path;

    const std::regex warns_on_tile_flag(R"(spdlog::warn\([^;\n]*tile_flag)");
    EXPECT_TRUE(std::regex_search(content, warns_on_tile_flag))
        << "LevelLoader 在解析 tileset tile_flag 时，应对未知 token 输出 warn（携带 tileset 路径与 tile local id），以便排查地图配置拼写错误。";
}

} // namespace
} // namespace engine::loader
// NOLINTEND

