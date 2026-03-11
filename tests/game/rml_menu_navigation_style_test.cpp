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
    EXPECT_NE(source.find(".tf-button-primary:hover,\n.tf-button-primary:focus"), std::string::npos);
    EXPECT_NE(source.find(".tf-button-secondary:hover,\n.tf-button-secondary:focus"), std::string::npos);
    EXPECT_NE(source.find(".tf-icon-button:hover,\n.tf-icon-button:focus"), std::string::npos);
}

TEST(RmlMenuNavigationStyleTest, InventoryMenuSlotsExposeFocusableNavigationAndActionMenuBindings) {
    const std::filesystem::path rml_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "ui/rmlui/scenes/inventory_menu.rml").lexically_normal();
    const std::filesystem::path style_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "ui/rmlui/scenes/inventory_menu.rcss").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(rml_path)) << rml_path;
    ASSERT_TRUE(std::filesystem::exists(style_path)) << style_path;

    const std::string rml_source = readTextFile(rml_path);
    const std::string style_source = readTextFile(style_path);
    ASSERT_FALSE(rml_source.empty());
    ASSERT_FALSE(style_source.empty());

    EXPECT_NE(style_source.find("body {\n    width: 100%;\n    height: 100%;\n    nav: auto;"), std::string::npos);
    EXPECT_NE(style_source.find(".inventory-menu-slot-button {\n    position: relative;\n    width: 20dp;\n    height: 20dp;"), std::string::npos);
    EXPECT_NE(style_source.find("tab-index: auto;"), std::string::npos);
    EXPECT_NE(style_source.find("nav-up: auto;"), std::string::npos);
    EXPECT_NE(style_source.find("nav-down: auto;"), std::string::npos);
    EXPECT_NE(style_source.find("nav-left: auto;"), std::string::npos);
    EXPECT_NE(style_source.find("nav-right: auto;"), std::string::npos);
    EXPECT_NE(style_source.find(".inventory-menu-slot-button:focus"), std::string::npos);
    EXPECT_NE(style_source.find(".inventory-menu-slot-button.draggable"), std::string::npos);
    EXPECT_NE(style_source.find("drag: clone;"), std::string::npos);

    EXPECT_NE(rml_source.find("data-event-click=\"slot_confirm(slot.slot_index)\""), std::string::npos);
    EXPECT_NE(rml_source.find("data-visible=\"action_menu_open\""), std::string::npos);
    EXPECT_NE(rml_source.find("data-style-left=\"action_menu_left\""), std::string::npos);
    EXPECT_NE(rml_source.find("data-style-top=\"action_menu_top\""), std::string::npos);
    EXPECT_NE(rml_source.find("data-class-draggable=\"slot.can_drag\""), std::string::npos);
    EXPECT_NE(rml_source.find("data-class-disabled=\"!can_use_selected\""), std::string::npos);
    EXPECT_NE(rml_source.find("data-class-disabled=\"!can_discard_selected\""), std::string::npos);
}

} // namespace
} // namespace game::ui
// NOLINTEND
