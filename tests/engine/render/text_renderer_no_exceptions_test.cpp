// NOLINTBEGIN
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

namespace engine::render {
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

TEST(TextRendererNoExceptionsTest, TextRendererCppHasNoTryCatch) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/render/text_renderer.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string content = readTextFile(source_path);
    ASSERT_FALSE(content.empty()) << "无法读取: " << source_path;
    EXPECT_FALSE(containsTryCatch(content));
}

} // namespace
} // namespace engine::render
// NOLINTEND

