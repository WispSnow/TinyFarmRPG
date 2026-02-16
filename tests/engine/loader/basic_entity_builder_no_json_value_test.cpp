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

TEST(BasicEntityBuilderNoJsonValueTest, BuilderDoesNotUseJsonValueAccessor) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/loader/basic_entity_builder.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string content = readTextFile(source_path);
    ASSERT_FALSE(content.empty()) << "无法读取: " << source_path;

    EXPECT_EQ(content.find(".value(\""), std::string::npos)
        << "BasicEntityBuilder 读取对象属性时不应使用 json.value(\"...\")，避免类型不匹配导致异常/终止。";
}

} // namespace
} // namespace engine::loader
// NOLINTEND

