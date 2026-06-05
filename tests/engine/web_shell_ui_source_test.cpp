// NOLINTBEGIN
#include <gtest/gtest.h>

#include <filesystem>
#include <string>

#include "render/test_source_utils.h"

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

namespace {

[[nodiscard]] std::string readWebShellUiSource() {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/web/web_shell_ui.cpp").lexically_normal();
    EXPECT_TRUE(std::filesystem::exists(source_path)) << source_path;
    return test_source_utils::readTextFile(source_path);
}

TEST(WebShellUiSourceTest, AudioContextIsCreatedFromClickHandler) {
    const std::string source = readWebShellUiSource();
    ASSERT_FALSE(source.empty());

    const std::size_t unlock_function = source.find("const unlockAudio = async () =>");
    const std::size_t context_create = source.find("new AudioContextCtor()");
    const std::size_t click_listener = source.find("audioButton.addEventListener(\"click\", unlockAudio);");

    ASSERT_NE(unlock_function, std::string::npos);
    ASSERT_NE(context_create, std::string::npos);
    ASSERT_NE(click_listener, std::string::npos);
    EXPECT_GT(context_create, unlock_function);
    EXPECT_GT(click_listener, context_create);
}

TEST(WebShellUiSourceTest, KeepsWebGlFeatureProbeForRuntimePostProcessingGate) {
    const std::string source = readWebShellUiSource();
    ASSERT_FALSE(source.empty());

    EXPECT_NE(source.find("tf_web_shell_report_webgl_features"), std::string::npos);
    EXPECT_NE(source.find("getExtension(\"EXT_color_buffer_float\")"), std::string::npos);
    EXPECT_NE(source.find("getExtension(\"OES_texture_float_linear\")"), std::string::npos);
    EXPECT_NE(source.find("Bloom use runtime diagnostics"), std::string::npos);
}

} // namespace
// NOLINTEND
