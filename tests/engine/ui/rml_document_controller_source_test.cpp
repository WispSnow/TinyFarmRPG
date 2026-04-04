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

TEST(RmlDocumentControllerSourceTest, HeaderKeepsLifecycleAndBindingApiUsedByCurrentScenes) {
    const std::filesystem::path header_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/ui/rmlui/rml_document_controller.h").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(header_path)) << header_path;

    const std::string source = test_source_utils::readTextFile(header_path);
    ASSERT_FALSE(source.empty()) << "无法读取: " << header_path;

    EXPECT_NE(source.find("void attach("), std::string::npos);
    EXPECT_NE(source.find("createModel("), std::string::npos);
    EXPECT_NE(source.find("load("), std::string::npos);
    EXPECT_NE(source.find("void unload()"), std::string::npos);
    EXPECT_NE(source.find("markDirty("), std::string::npos);
    EXPECT_NE(source.find("markAllDirty()"), std::string::npos);
    EXPECT_NE(source.find("isModelValid()"), std::string::npos);
    EXPECT_NE(source.find("document() const"), std::string::npos);
    EXPECT_NE(source.find("runtime() const"), std::string::npos);

    EXPECT_EQ(source.find("enableHoverFocusSync("), std::string::npos);
    EXPECT_EQ(source.find("setDefaultFocusById("), std::string::npos);
    EXPECT_EQ(source.find("setDefaultFocusFirstEnabledByClass("), std::string::npos);
    EXPECT_EQ(source.find("queueDefaultFocus()"), std::string::npos);
    EXPECT_EQ(source.find("queueFocusElement("), std::string::npos);
    EXPECT_EQ(source.find("queueFocusElementById("), std::string::npos);
    EXPECT_EQ(source.find("queueFocusFirstEnabledElementByClass("), std::string::npos);
    EXPECT_EQ(source.find("HoverFocusSyncListener"), std::string::npos);
    EXPECT_EQ(source.find("disableHoverFocusSync("), std::string::npos);
    EXPECT_EQ(source.find("clearDefaultFocus("), std::string::npos);
    EXPECT_EQ(source.find("void show()"), std::string::npos);
    EXPECT_EQ(source.find("void hide()"), std::string::npos);
    EXPECT_EQ(source.find("destroyModel("), std::string::npos);
}

} // namespace
} // namespace engine::ui::rmlui
// NOLINTEND
