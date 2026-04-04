// NOLINTBEGIN
#include <gtest/gtest.h>

#include <filesystem>
#include <string>

#include "../engine/render/test_source_utils.h"

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

namespace game::ui {
namespace {

TEST(HudDocumentControllerSourceTest, TimeClockHudUsesDocumentControllerInsteadOfOwningDataBridge) {
    const std::filesystem::path header_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/ui/time_clock_hud.h").lexically_normal();
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/ui/time_clock_hud.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(header_path)) << header_path;
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string header = test_source_utils::readTextFile(header_path);
    const std::string source = test_source_utils::readTextFile(source_path);
    ASSERT_FALSE(header.empty()) << "无法读取: " << header_path;
    ASSERT_FALSE(source.empty()) << "无法读取: " << source_path;

    EXPECT_NE(header.find("RmlDocumentController document_controller_"), std::string::npos);
    EXPECT_EQ(header.find("RmlDataBridge"), std::string::npos);
    EXPECT_NE(source.find("document_controller_.createModel(MODEL_NAME)"), std::string::npos);
    EXPECT_NE(source.find("document_controller_.load(DOCUMENT_PATH)"), std::string::npos);
    EXPECT_NE(source.find("document_controller_.markDirty(\"day_text\")"), std::string::npos);
    EXPECT_NE(source.find("document_controller_.markDirty(\"time_text\")"), std::string::npos);
    EXPECT_NE(source.find("document_controller_.markDirty(\"hand_decorator\")"), std::string::npos);
}

TEST(HudDocumentControllerSourceTest, HotbarUiUsesDocumentControllerForModelAndDocumentLifecycle) {
    const std::filesystem::path header_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/ui/hotbar_ui.h").lexically_normal();
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/game/ui/hotbar_ui.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(header_path)) << header_path;
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string header = test_source_utils::readTextFile(header_path);
    const std::string source = test_source_utils::readTextFile(source_path);
    ASSERT_FALSE(header.empty()) << "无法读取: " << header_path;
    ASSERT_FALSE(source.empty()) << "无法读取: " << source_path;

    EXPECT_NE(header.find("RmlDocumentController document_controller_"), std::string::npos);
    EXPECT_EQ(header.find("RmlDataBridge"), std::string::npos);
    EXPECT_NE(source.find("document_controller_.createModel(MODEL_NAME, &type_register_)"), std::string::npos);
    EXPECT_NE(source.find("document_controller_.load(DOCUMENT_PATH)"), std::string::npos);
    EXPECT_NE(source.find("document_controller_.markDirty(\"hotbar_slots\")"), std::string::npos);
    EXPECT_NE(source.find("document_controller_.unload();"), std::string::npos);
}

} // namespace
} // namespace game::ui
// NOLINTEND
