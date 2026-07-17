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

TEST(ToolsCMakeSourceTest, DefaultBuildOnlyEnablesGameTargets) {
    const std::filesystem::path cmake_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "CMakeLists.txt").lexically_normal();
    const std::string source = readTextFile(cmake_path);
    ASSERT_FALSE(source.empty()) << "无法读取: " << cmake_path;

    EXPECT_NE(source.find("option(BUILD_TOOLS \"是否构建工具\" OFF)"), std::string::npos);
    EXPECT_NE(source.find("option(BUILD_LEARN \"构建学习/实验目标\" OFF)"), std::string::npos);
    EXPECT_NE(source.find("option(BUILD_TESTING \"是否构建测试\" OFF)"), std::string::npos);
}

TEST(ToolsCMakeSourceTest, WindowsSubsystemOnlyAppliesToGameExecutable) {
    const std::filesystem::path root_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "CMakeLists.txt").lexically_normal();
    const std::filesystem::path compiler_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "cmake/CompilerSettings.cmake")
            .lexically_normal();
    const std::string root_source = readTextFile(root_path);
    const std::string compiler_source = readTextFile(compiler_path);
    ASSERT_FALSE(root_source.empty()) << "无法读取: " << root_path;
    ASSERT_FALSE(compiler_source.empty()) << "无法读取: " << compiler_path;

    EXPECT_NE(root_source.find("add_executable(${TARGET} WIN32)"), std::string::npos);
    EXPECT_EQ(compiler_source.find("/SUBSYSTEM:WINDOWS"), std::string::npos);
    EXPECT_EQ(compiler_source.find("/ENTRY:mainCRTStartup"), std::string::npos);
    EXPECT_NE(compiler_source.find("/MP"), std::string::npos);
}

TEST(ToolsCMakeSourceTest, RuntimeFilesAreBuildDependencies) {
    const std::filesystem::path cmake_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "cmake/BuildHelpers.cmake")
            .lexically_normal();
    const std::string source = readTextFile(cmake_path);
    ASSERT_FALSE(source.empty()) << "无法读取: " << cmake_path;

    EXPECT_NE(source.find("add_custom_target(${SYNC_TARGET}"), std::string::npos);
    EXPECT_NE(source.find("add_dependencies(${TARGET_NAME} ${SYNC_TARGET})"), std::string::npos);
    EXPECT_EQ(source.find("PRE_BUILD"), std::string::npos);
}

TEST(ToolsCMakeSourceTest, TestExecutablesDeployWindowsRuntimeDlls) {
    const std::filesystem::path cmake_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "tests/CMakeLists.txt").lexically_normal();
    const std::string source = readTextFile(cmake_path);
    ASSERT_FALSE(source.empty()) << "无法读取: " << cmake_path;

    EXPECT_NE(source.find("setup_windows_dll_copy(engine_tests)"), std::string::npos);
    EXPECT_NE(source.find("setup_windows_dll_copy(game_tests)"), std::string::npos);
}

// NOLINTEND
