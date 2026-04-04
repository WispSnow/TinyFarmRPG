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

TEST(RmlScreenFadeTransitionSourceTest, SourceUsesTransitionendAndDynamicTransitionProperty) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/ui/rmlui/rml_screen_fade.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = test_source_utils::readTextFile(source_path);
    ASSERT_FALSE(source.empty()) << "无法读取: " << source_path;

    EXPECT_NE(source.find("transitionend"), std::string::npos);
    EXPECT_NE(source.find("event.GetParameter<Rml::String>(\"property\", \"\")"), std::string::npos);
    EXPECT_NE(source.find("overlay_->SetProperty(\"transition\", \"none\");"), std::string::npos);
    EXPECT_NE(source.find("overlay_->SetProperty(\"transition\", std::format(\"opacity {:.3f}s linear-in-out\", duration));"),
              std::string::npos);
    EXPECT_NE(source.find("overlay_->SetClass(Rml::String{OPAQUE_CLASS.data(), OPAQUE_CLASS.size()}, opaque);"),
              std::string::npos);
    EXPECT_EQ(source.find("applyOpacity("), std::string::npos);
    EXPECT_EQ(source.find("alpha_"), std::string::npos);
    EXPECT_EQ(source.find("armTransitionWatchdog("), std::string::npos);
    EXPECT_EQ(source.find("clearTransitionWatchdog("), std::string::npos);
}

TEST(RmlScreenFadeTransitionSourceTest, HeaderKeepsOnlyMinimalTransitionState) {
    const std::filesystem::path header_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/ui/rmlui/rml_screen_fade.h").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(header_path)) << header_path;

    const std::string header = test_source_utils::readTextFile(header_path);
    ASSERT_FALSE(header.empty()) << "无法读取: " << header_path;

    EXPECT_NE(header.find("bool transition_listener_registered_{false};"), std::string::npos);
    EXPECT_NE(header.find("bool overlay_opaque_{false};"), std::string::npos);
    EXPECT_EQ(header.find("alpha_"), std::string::npos);
    EXPECT_EQ(header.find("from_alpha_"), std::string::npos);
    EXPECT_EQ(header.find("to_alpha_"), std::string::npos);
    EXPECT_EQ(header.find("transition_pending_"), std::string::npos);
    EXPECT_EQ(header.find("transition_duration_"), std::string::npos);
    EXPECT_EQ(header.find("transition_elapsed_"), std::string::npos);
}

TEST(RmlScreenFadeTransitionSourceTest, OverlayRcssDefinesOpaqueClassAndTransitionBaseline) {
    const std::filesystem::path rcss_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "ui/rmlui/overlay/screen_fade.rcss").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(rcss_path)) << rcss_path;

    const std::string rcss = test_source_utils::readTextFile(rcss_path);
    ASSERT_FALSE(rcss.empty()) << "无法读取: " << rcss_path;

    EXPECT_NE(rcss.find("transition: none;"), std::string::npos);
    EXPECT_NE(rcss.find("#fade-overlay.is-opaque"), std::string::npos);
    EXPECT_NE(rcss.find("opacity: 1;"), std::string::npos);
}

} // namespace
} // namespace engine::ui::rmlui
// NOLINTEND
