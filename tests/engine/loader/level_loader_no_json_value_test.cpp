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

TEST(LevelLoaderNoJsonValueTest, LevelLoaderDoesNotUseJsonValueAccessor) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/loader/level_loader.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string content = readTextFile(source_path);
    ASSERT_FALSE(content.empty()) << "无法读取: " << source_path;

    EXPECT_EQ(content.find(".value(\""), std::string::npos)
        << "LevelLoader 解析外部 JSON（地图/tileset）时不应使用 json.value(\"...\")，避免类型不匹配导致异常/终止。";

    EXPECT_EQ(content.find("->value(\""), std::string::npos)
        << "LevelLoader 解析外部 JSON（地图/tileset）时不应使用 json->value(\"...\")，避免类型不匹配导致异常/终止。";

	    EXPECT_EQ(content.find(".get<int>"), std::string::npos)
	        << "LevelLoader 解析外部 JSON（地图/tileset）时应避免 json.get<int>()（可能触发 out_of_range/type_error）。";

	    const std::regex implicit_int_range_for(R"(for\s*\(\s*(?:const\s+)?int\s+\w+\s*:\s*data\s*\))");
	    EXPECT_FALSE(std::regex_search(content, implicit_int_range_for))
	        << "LevelLoader 不应直接用 range-for 的 int 变量遍历 JSON 数组（可能触发隐式 get<int>()），应改用 get_ptr + 安全转换。";
	}

} // namespace
} // namespace engine::loader
// NOLINTEND
