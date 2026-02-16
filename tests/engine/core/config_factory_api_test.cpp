#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include "engine/core/config.h"

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

namespace {
[[nodiscard]] std::string readTextFile(const std::filesystem::path& path) {
    std::ifstream file(path);
    EXPECT_TRUE(file.is_open()) << path;
    std::string content(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>()
    );
    return content;
}
} // namespace

namespace engine::core {

TEST(ConfigFactoryApiTest, CreateReturnsUniquePtr) {
    using ReturnType = decltype(Config::create(std::declval<std::string_view>()));
    static_assert(std::is_same_v<ReturnType, std::unique_ptr<Config>>);
    SUCCEED();
}

TEST(ConfigNoExceptionsTest, ConfigCppHasNoTryCatch) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/core/config.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = readTextFile(source_path);
    ASSERT_FALSE(source.empty());
    EXPECT_EQ(source.find("try"), std::string::npos);
    EXPECT_EQ(source.find("catch"), std::string::npos);
}

} // namespace engine::core
