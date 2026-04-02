// NOLINTBEGIN
#include <gtest/gtest.h>

#include <filesystem>
#include <string>

#include "../render/test_source_utils.h"

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

namespace engine::ui::rmlui {
namespace {

TEST(RmlElementHelpersSourceTest, HeaderKeepsOnlyMinimalHelpersNeededByCurrentUiArchitecture) {
    const std::filesystem::path header_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/ui/rmlui/rml_element_helpers.h").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(header_path)) << header_path;

    const std::string source = test_source_utils::readTextFile(header_path);
    ASSERT_FALSE(source.empty()) << "无法读取: " << header_path;

    EXPECT_NE(source.find("snapToPixel"), std::string::npos);
    EXPECT_NE(source.find("toPixelString"), std::string::npos);
    EXPECT_NE(source.find("setPixelProperty"), std::string::npos);
    EXPECT_NE(source.find("textToInnerRml"), std::string::npos);

    EXPECT_EQ(source.find("setPaddingProperties"), std::string::npos);
    EXPECT_EQ(source.find("setFontSizeProperty"), std::string::npos);
    EXPECT_EQ(source.find("computeLineSpacingScale"), std::string::npos);
    EXPECT_EQ(source.find("getComputedPadding"), std::string::npos);
    EXPECT_EQ(source.find("getComputedFontSize"), std::string::npos);
    EXPECT_EQ(source.find("getComputedLineHeight"), std::string::npos);
    EXPECT_EQ(source.find("getComputedWidth"), std::string::npos);
    EXPECT_EQ(source.find("getComputedHeight"), std::string::npos);
    EXPECT_EQ(source.find("getComputedMaxWidth"), std::string::npos);
    EXPECT_EQ(source.find("getComputedMarginBottom"), std::string::npos);
}

} // namespace
} // namespace engine::ui::rmlui
// NOLINTEND
