// NOLINTBEGIN
#include <gtest/gtest.h>

#include "../render/test_source_utils.h"

#include <filesystem>
#include <string>

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

namespace engine::vfx {
namespace {

TEST(VfxOptionalBackendSourceTest, CMakeGuardsEffekseerBackendBehindOption) {
    const auto root_cmake_path = (std::filesystem::path{PROJECT_SOURCE_DIR} / "CMakeLists.txt").lexically_normal();
    const auto src_cmake_path = (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/CMakeLists.txt").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(root_cmake_path)) << root_cmake_path;
    ASSERT_TRUE(std::filesystem::exists(src_cmake_path)) << src_cmake_path;

    const std::string root_cmake = test_source_utils::readTextFile(root_cmake_path);
    const std::string src_cmake = test_source_utils::readTextFile(src_cmake_path);
    ASSERT_FALSE(root_cmake.empty());
    ASSERT_FALSE(src_cmake.empty());

    EXPECT_NE(root_cmake.find("option(ENABLE_EFFEKSEER"), std::string::npos);
    EXPECT_NE(root_cmake.find("if(ENABLE_EFFEKSEER)\n    setup_effekseer_dependencies()"), std::string::npos);
    EXPECT_NE(root_cmake.find("target_compile_definitions(engine PUBLIC TF_ENABLE_EFFEKSEER)"), std::string::npos);
    EXPECT_NE(root_cmake.find("EffekseerRendererGL"), std::string::npos);
    EXPECT_NE(src_cmake.find("engine/vfx/effekseer_backend_factory.cpp"), std::string::npos);
    EXPECT_NE(src_cmake.find("if(ENABLE_EFFEKSEER)\n    target_sources(engine PRIVATE"), std::string::npos);
    EXPECT_NE(src_cmake.find("engine/vfx/effekseer_backend.cpp"), std::string::npos);
}

TEST(VfxOptionalBackendSourceTest, FactoryReturnsNullWhenEffekseerIsDisabled) {
    const auto factory_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/vfx/effekseer_backend_factory.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(factory_path)) << factory_path;

    const std::string source = test_source_utils::readTextFile(factory_path);
    ASSERT_FALSE(source.empty());

    EXPECT_NE(source.find("#ifdef TF_ENABLE_EFFEKSEER"), std::string::npos);
    EXPECT_NE(source.find("return EffekseerBackend::create();"), std::string::npos);
    EXPECT_NE(source.find("return nullptr;"), std::string::npos);
}

} // namespace
} // namespace engine::vfx
// NOLINTEND
