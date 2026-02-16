// NOLINTBEGIN
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

namespace engine::core {
namespace {

[[nodiscard]] std::string readTextFile(const std::filesystem::path& path) {
    std::ifstream file(path);
    EXPECT_TRUE(file.is_open()) << path;
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

TEST(GameAppDispatcherTraceTest, RunLoopMarksQueueDispatchForUpdate) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/core/game_app.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = readTextFile(source_path);
    ASSERT_FALSE(source.empty());

    EXPECT_EQ(source.find("debug_ui_manager_->onFrameBegin()"), std::string::npos)
        << "Frame boundary hook should be removed now that DispatcherTrace tracks recent dispatches instead of per-frame counters.";
    EXPECT_NE(source.find("debug_ui_manager_->onDispatcherUpdateBegin()"), std::string::npos)
        << "GameApp should allow the debug UI to observe dispatcher.update() scope (begin).";
    EXPECT_NE(source.find("debug_ui_manager_->onDispatcherUpdateEnd()"), std::string::npos)
        << "GameApp should allow the debug UI to observe dispatcher.update() scope (end).";
}

} // namespace
} // namespace engine::core
// NOLINTEND
