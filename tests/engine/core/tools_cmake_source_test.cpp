// NOLINTBEGIN
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

namespace {

[[nodiscard]] std::string readTextFile(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return {};
    }
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

} // namespace

TEST(ToolsCMakeSourceTest, TopLevelBuildToolsDoesNotRequireDebugUi) {
    const std::filesystem::path cmake_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "CMakeLists.txt").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(cmake_path)) << cmake_path;

    const std::string source = readTextFile(cmake_path);
    ASSERT_FALSE(source.empty()) << "无法读取: " << cmake_path;

    EXPECT_NE(source.find("if(BUILD_TOOLS)"), std::string::npos);
    EXPECT_EQ(source.find("if(BUILD_TOOLS AND ENABLE_DEBUG_UI)"), std::string::npos);
}

TEST(ToolsCMakeSourceTest, OnlyImguiDrivenToolsRequireDebugUi) {
    const std::filesystem::path cmake_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "tools/CMakeLists.txt").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(cmake_path)) << cmake_path;

    const std::string source = readTextFile(cmake_path);
    ASSERT_FALSE(source.empty()) << "无法读取: " << cmake_path;

    const std::size_t visual_guard = source.find("if(ENABLE_DEBUG_UI)");
    const std::size_t visual_target = source.find("add_executable(visual_tester");
    const std::size_t rmlui_guard = source.find("if(ENABLE_DEBUG_UI AND BUILD_RMLUI_TESTER)");
    const std::size_t rmlui_target = source.find("add_executable(rmlui_tester");
    const std::size_t battle_target = source.find("add_executable(battle_tester");
    const std::size_t scheduler_target = source.find("add_executable(scheduler_dot_dump");

    ASSERT_NE(visual_guard, std::string::npos);
    ASSERT_NE(visual_target, std::string::npos);
    ASSERT_NE(rmlui_guard, std::string::npos);
    ASSERT_NE(rmlui_target, std::string::npos);
    ASSERT_NE(battle_target, std::string::npos);
    ASSERT_NE(scheduler_target, std::string::npos);

    EXPECT_LT(visual_guard, visual_target);
    EXPECT_LT(rmlui_guard, rmlui_target);
    EXPECT_LT(rmlui_target, battle_target);
    EXPECT_LT(battle_target, scheduler_target);
}

// NOLINTEND
