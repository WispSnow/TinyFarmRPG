// NOLINTBEGIN
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

namespace engine::resource {
namespace {

[[nodiscard]] std::string readTextFile(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return {};
    }
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

TEST(ResourceManagerFilesystemNoExceptionsTest, LoadResourcesDoesNotCallThrowingFilesystemExists) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/resource/resource_manager.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string content = readTextFile(source_path);
    ASSERT_FALSE(content.empty()) << "无法读取: " << source_path;

    EXPECT_EQ(content.find("std::filesystem::exists(path)"), std::string::npos)
        << "std::filesystem::exists(path) 可能抛出 filesystem_error；禁用异常后会导致终止，应改用 error_code 重载。";
}

} // namespace
} // namespace engine::resource
// NOLINTEND

