// NOLINTBEGIN
#include <gtest/gtest.h>

#include <filesystem>
#include <string>

#include "render/test_source_utils.h"

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

namespace {

[[nodiscard]] std::filesystem::path projectPath(const char* relative_path) {
    return (std::filesystem::path{PROJECT_SOURCE_DIR} / relative_path).lexically_normal();
}

[[nodiscard]] std::string readProjectFile(const char* relative_path) {
    const std::filesystem::path path = projectPath(relative_path);
    EXPECT_TRUE(std::filesystem::exists(path)) << path;
    return test_source_utils::readTextFile(path);
}

TEST(WebGameplayTargetSourceTest, ReusesSharedSdlCallbackMain) {
    const std::string root_cmake = readProjectFile("CMakeLists.txt");
    const std::string main_source = readProjectFile("src/main.cpp");

    ASSERT_FALSE(root_cmake.empty());
    ASSERT_FALSE(main_source.empty());

    EXPECT_FALSE(std::filesystem::exists(projectPath("src/web/web_game_main.cpp")));
    EXPECT_NE(root_cmake.find("if(TF_BUILD_WEB AND TF_BUILD_WEB_SKELETON)"), std::string::npos);
    EXPECT_NE(root_cmake.find("tf_configure_web_executable(${TARGET})"), std::string::npos);
    EXPECT_NE(main_source.find("SDL_MAIN_USE_CALLBACKS"), std::string::npos);
    EXPECT_NE(main_source.find("SDL_AppInit"), std::string::npos);
    EXPECT_NE(main_source.find("SDL_AppIterate"), std::string::npos);
    EXPECT_NE(main_source.find("SDL_AppEvent"), std::string::npos);
    EXPECT_NE(main_source.find("SDL_AppQuit"), std::string::npos);
}

TEST(WebGameplayTargetSourceTest, WebRuntimeOwnsExceptionAndLinkPolicy) {
    const std::string runtime_cmake = readProjectFile("cmake/WebRuntime.cmake");

    ASSERT_FALSE(runtime_cmake.empty());

    EXPECT_NE(runtime_cmake.find("-fno-exceptions"), std::string::npos);
    EXPECT_NE(runtime_cmake.find("SPDLOG_NO_EXCEPTIONS"), std::string::npos);
    EXPECT_NE(runtime_cmake.find("JSON_NOEXCEPTION"), std::string::npos);
    EXPECT_NE(runtime_cmake.find("SOL_NO_EXCEPTIONS=1"), std::string::npos);
    EXPECT_NE(runtime_cmake.find("-sDISABLE_EXCEPTION_CATCHING=1"), std::string::npos);
    EXPECT_NE(runtime_cmake.find("-sMIN_WEBGL_VERSION=2"), std::string::npos);
    EXPECT_NE(runtime_cmake.find("-sMAX_WEBGL_VERSION=2"), std::string::npos);
    EXPECT_NE(runtime_cmake.find("-sFORCE_FILESYSTEM=1"), std::string::npos);
    EXPECT_NE(runtime_cmake.find("-lidbfs.js"), std::string::npos);
    EXPECT_NE(runtime_cmake.find("if(TF_WEB_ENABLE_PTHREADS)"), std::string::npos);
}

TEST(WebGameplayTargetSourceTest, WebDependenciesAvoidDesktopGlAndImageLibraries) {
    const std::string root_cmake = readProjectFile("CMakeLists.txt");
    const std::string dependencies_cmake = readProjectFile("cmake/WebDependencies.cmake");

    ASSERT_FALSE(root_cmake.empty());
    ASSERT_FALSE(dependencies_cmake.empty());

    const std::size_t desktop_link_guard = root_cmake.find("if(NOT TF_BUILD_WEB)");
    const std::size_t sdl_image_link = root_cmake.find("SDL3_image::SDL3_image");
    const std::size_t opengl_link = root_cmake.find("OpenGL::GL");
    const std::size_t glad_link = root_cmake.find("glad");

    ASSERT_NE(desktop_link_guard, std::string::npos);
    ASSERT_NE(sdl_image_link, std::string::npos);
    ASSERT_NE(opengl_link, std::string::npos);
    ASSERT_NE(glad_link, std::string::npos);
    EXPECT_LT(desktop_link_guard, sdl_image_link);
    EXPECT_LT(desktop_link_guard, opengl_link);
    EXPECT_LT(desktop_link_guard, glad_link);

    EXPECT_NE(dependencies_cmake.find("add_library(tf_web_sdl3 INTERFACE)"), std::string::npos);
    EXPECT_NE(dependencies_cmake.find("add_library(SDL3::SDL3 ALIAS tf_web_sdl3)"), std::string::npos);
    EXPECT_NE(dependencies_cmake.find("-sUSE_SDL=3"), std::string::npos);
    EXPECT_NE(dependencies_cmake.find("function(tf_web_add_lua)"), std::string::npos);
    EXPECT_NE(dependencies_cmake.find("function(tf_web_add_sol2)"), std::string::npos);
    EXPECT_NE(dependencies_cmake.find("SPDLOG_NO_EXCEPTIONS"), std::string::npos);
}

TEST(WebGameplayTargetSourceTest, WebGlPlatformGuardsSrgbAndDepthClear) {
    const std::string platform_header = readProjectFile("src/engine/platform/gl_platform.h");
    const std::string renderer_source = readProjectFile("src/engine/render/opengl/gl_renderer.cpp");

    ASSERT_FALSE(platform_header.empty());
    ASSERT_FALSE(renderer_source.empty());

    EXPECT_NE(platform_header.find("TF_GL_PLATFORM_WEBGL"), std::string::npos);
    EXPECT_NE(platform_header.find("kSupportsDefaultFramebufferSrgb = !kIsWebGL"), std::string::npos);
    EXPECT_NE(platform_header.find("kSupportsFloatColorFramebuffers = !kIsWebGL"), std::string::npos);
    EXPECT_NE(platform_header.find("kEnableHdrPostProcessingByDefault = kSupportsFloatColorFramebuffers"), std::string::npos);
    EXPECT_NE(platform_header.find("glClearDepthf(depth);"), std::string::npos);
    EXPECT_NE(platform_header.find("glClearDepth(static_cast<GLdouble>(depth));"), std::string::npos);
    EXPECT_NE(renderer_source.find("engine::platform::gl::kSupportsDefaultFramebufferSrgb"), std::string::npos);
    EXPECT_NE(renderer_source.find("if constexpr (engine::platform::gl::kEnableHdrPostProcessingByDefault)"),
              std::string::npos);
    EXPECT_NE(renderer_source.find("bloom_enabled_ && bloom_pass_ && emissive_pass_"), std::string::npos);
    EXPECT_NE(renderer_source.find("engine::platform::gl::clearDepth(1.0f);"), std::string::npos);
    EXPECT_EQ(renderer_source.find("glClearDepth(1.0);"), std::string::npos);
}

} // namespace
// NOLINTEND
