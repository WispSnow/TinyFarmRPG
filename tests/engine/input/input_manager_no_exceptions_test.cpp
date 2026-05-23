// NOLINTBEGIN
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

namespace engine::input {
namespace {

[[nodiscard]] std::string readTextFile(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return {};
    }
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

[[nodiscard]] bool containsTryCatch(const std::string& source) {
    if (source.find("try {") != std::string::npos) {
        return true;
    }
    if (source.find("try\n{") != std::string::npos) {
        return true;
    }
    if (source.find("catch (") != std::string::npos) {
        return true;
    }
    if (source.find("catch(") != std::string::npos) {
        return true;
    }
    return false;
}

TEST(InputManagerNoExceptionsTest, InputSourcesHaveNoTryCatch) {
    const std::filesystem::path source_root =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/input").lexically_normal();
    const std::vector<std::filesystem::path> source_paths{
        source_root / "input_manager.cpp",
        source_root / "input_binding_config.cpp",
        source_root / "input_binding_tokens.cpp",
        source_root / "input_context_registry.cpp",
        source_root / "input_event_routing.cpp",
    };

    for (const auto& source_path : source_paths) {
        ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

        const std::string content = readTextFile(source_path);
        ASSERT_FALSE(content.empty()) << "无法读取: " << source_path;
        EXPECT_FALSE(containsTryCatch(content)) << source_path;
    }
}

} // namespace
} // namespace engine::input
// NOLINTEND
