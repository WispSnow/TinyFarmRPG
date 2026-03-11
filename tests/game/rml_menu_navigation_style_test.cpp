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
    EXPECT_TRUE(file.is_open()) << path;
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

} // namespace

namespace game::ui {
namespace {

TEST(RmlMenuNavigationStyleTest, MenuWidgetsExposeDirectionalNavigationAndFocusStyle) {
    const std::filesystem::path style_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "ui/rmlui/theme/menu_widgets.rcss").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(style_path)) << style_path;

    const std::string source = readTextFile(style_path);
    ASSERT_FALSE(source.empty());

    EXPECT_NE(source.find("nav-up: auto;"), std::string::npos);
    EXPECT_NE(source.find("nav-down: auto;"), std::string::npos);
    EXPECT_NE(source.find("nav-left: auto;"), std::string::npos);
    EXPECT_NE(source.find("nav-right: auto;"), std::string::npos);
    EXPECT_NE(source.find(".tf-button-primary:focus"), std::string::npos);
    EXPECT_NE(source.find(".tf-button-secondary:focus"), std::string::npos);
    EXPECT_NE(source.find(".tf-icon-button:focus"), std::string::npos);
}

} // namespace
} // namespace game::ui
// NOLINTEND
