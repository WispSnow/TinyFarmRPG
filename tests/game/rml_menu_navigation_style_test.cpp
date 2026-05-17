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

TEST(RmlMenuNavigationStyleTest, MenuWidgetsExposeDirectionalNavigationAndUnifiedHoverFocusStyle) {
    const std::filesystem::path style_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "ui/rmlui/theme/menu_widgets.rcss").lexically_normal();
    const std::filesystem::path nav_style_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "ui/rmlui/theme/nav.rcss").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(style_path)) << style_path;
    ASSERT_TRUE(std::filesystem::exists(nav_style_path)) << nav_style_path;

    const std::string source = readTextFile(style_path);
    const std::string nav_source = readTextFile(nav_style_path);
    ASSERT_FALSE(source.empty());
    ASSERT_FALSE(nav_source.empty());

    EXPECT_NE(source.find("nav-up: auto;"), std::string::npos);
    EXPECT_NE(source.find("nav-down: auto;"), std::string::npos);
    EXPECT_NE(source.find("nav-left: auto;"), std::string::npos);
    EXPECT_NE(source.find("nav-right: auto;"), std::string::npos);
    EXPECT_NE(source.find(".tf-input-mouse .tf-button-primary:hover,\n.tf-input-nav .tf-button-primary:focus"),
              std::string::npos);
    EXPECT_NE(source.find(".tf-input-mouse .tf-button-secondary:hover,\n.tf-input-nav .tf-button-secondary:focus"),
              std::string::npos);
    EXPECT_NE(source.find(".tf-input-mouse .tf-icon-button:hover,\n.tf-input-nav .tf-icon-button:focus"),
              std::string::npos);
    EXPECT_NE(source.find(".tf-input-mouse .tf-button-primary:hover:active,\n.tf-input-nav .tf-button-primary:focus:active"),
              std::string::npos);
    EXPECT_NE(source.find(".tf-input-mouse .tf-button-secondary:hover:active,\n.tf-input-nav .tf-button-secondary:focus:active"),
              std::string::npos);
    EXPECT_NE(source.find(".tf-input-mouse .tf-icon-button:hover:active,\n.tf-input-nav .tf-icon-button:focus:active"),
              std::string::npos);
    EXPECT_NE(source.find("transform: translateY(-1dp);"), std::string::npos);
    EXPECT_NE(source.find("transform: translateY(1dp);"), std::string::npos);
    EXPECT_NE(source.find("image-color: #fce97fff;"), std::string::npos);
    EXPECT_EQ(source.find("box-shadow:"), std::string::npos);

    EXPECT_NE(nav_source.find(".tf-input-mouse .tf-focus-ring-blue:hover,\n.tf-input-nav .tf-focus-ring-blue:focus"),
              std::string::npos);
    EXPECT_NE(nav_source.find(".tf-input-mouse .tf-focus-ring-gold:hover,\n.tf-input-nav .tf-focus-ring-gold:focus"),
              std::string::npos);
    EXPECT_NE(nav_source.find(".tf-input-mouse .tf-focus-ring-danger:hover,\n.tf-input-nav .tf-focus-ring-danger:focus"),
              std::string::npos);
    EXPECT_NE(nav_source.find(".tf-input-mouse .tf-focus-ring-gold:hover:active,\n.tf-input-nav .tf-focus-ring-gold:focus:active"),
              std::string::npos);
    EXPECT_NE(nav_source.find("image-color: #fce97fff;"), std::string::npos);
    EXPECT_EQ(nav_source.find("box-shadow:"), std::string::npos);
}

TEST(RmlMenuNavigationStyleTest, InventoryImageButtonsPreferPixelSafeTintOverFocusOutline) {
    const std::filesystem::path style_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "ui/rmlui/scenes/inventory_menu.rcss").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(style_path)) << style_path;

    const std::string source = readTextFile(style_path);
    ASSERT_FALSE(source.empty());

    EXPECT_NE(source.find(".tab-icon"), std::string::npos);
    EXPECT_NE(source.find("#trash-btn"), std::string::npos);
    EXPECT_NE(source.find("image-color: #d8dee9d0;"), std::string::npos);
    EXPECT_NE(source.find("transition: image-color 0.08s cubic-out;"), std::string::npos);
    EXPECT_EQ(source.find("transition: transform 0.08s cubic-out, image-color 0.08s cubic-out;"), std::string::npos);
    EXPECT_NE(source.find(".tf-input-mouse #trash-btn:hover,\n.tf-input-nav #trash-btn:focus"), std::string::npos);
    EXPECT_EQ(source.find("filter: saturate(1.12) brightness(1.08);"), std::string::npos);
    EXPECT_NE(source.find("filter: saturate(1.2) brightness(1.08);"), std::string::npos);
}

} // namespace
} // namespace game::ui
// NOLINTEND
