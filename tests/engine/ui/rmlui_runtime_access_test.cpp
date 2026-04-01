// NOLINTBEGIN
#include <gtest/gtest.h>

#include <filesystem>
#include <string>

#include "../render/test_source_utils.h"

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

namespace engine::core {
namespace {

TEST(RmlUiRuntimeAccessTest, ContextExposesRuntimeFromUiServices) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/core/context.h").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string content = test_source_utils::readTextFile(source_path);
    ASSERT_FALSE(content.empty()) << "无法读取: " << source_path;

    EXPECT_NE(content.find("engine::ui::rmlui::RmlUiRuntime* getRmlUi() const { return ui_.rmlui; }"),
              std::string::npos);
}

TEST(RmlUiRuntimeAccessTest, GameAppBuildsUiServicesFromOwnedRuntime) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/core/game_app.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string content = test_source_utils::readTextFile(source_path);
    ASSERT_FALSE(content.empty()) << "无法读取: " << source_path;

    const std::string init_context_block =
        test_source_utils::extractFunctionBlock(content, "bool GameApp::initContext()");
    ASSERT_FALSE(init_context_block.empty());

    EXPECT_NE(init_context_block.find("engine::core::UiServices ui_services{"), std::string::npos);
    EXPECT_NE(init_context_block.find("rmlui_runtime_.get()"), std::string::npos);
}

TEST(RmlUiRuntimeAccessTest, GameAppInitRmlUiUsesRenderHookWithoutCompatLayer) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/core/game_app.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string content = test_source_utils::readTextFile(source_path);
    ASSERT_FALSE(content.empty()) << "无法读取: " << source_path;

    const std::string init_rmlui_block =
        test_source_utils::extractFunctionBlock(content, "bool GameApp::initRmlUi()");
    ASSERT_FALSE(init_rmlui_block.empty());

    EXPECT_NE(init_rmlui_block.find("gl_renderer_->setRmlUiRenderHook("), std::string::npos);
    EXPECT_EQ(init_rmlui_block.find("RmlUILayer::createFacade("), std::string::npos);
    EXPECT_EQ(init_rmlui_block.find("setLegacyRmlUiLayer("), std::string::npos);
}

} // namespace
} // namespace engine::core

namespace engine::render::opengl {
namespace {

TEST(RmlUiRuntimeAccessTest, GlRendererHeaderContainsOnlyRenderHookBoundary) {
    const std::filesystem::path header_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/render/opengl/gl_renderer.h").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(header_path)) << header_path;

    const std::string content = test_source_utils::readTextFile(header_path);
    ASSERT_FALSE(content.empty()) << "无法读取: " << header_path;

    EXPECT_NE(content.find("void setRmlUiRenderHook(RmlUiRenderHook hook);"), std::string::npos);
    EXPECT_EQ(content.find("getRmlUILayer("), std::string::npos);
    EXPECT_EQ(content.find("setLegacyRmlUiLayer("), std::string::npos);
    EXPECT_EQ(content.find("handleRmlUiEvent("), std::string::npos);
    EXPECT_EQ(content.find("loadRmlUiDocument("), std::string::npos);
    EXPECT_EQ(content.find("reloadRmlUiDocument("), std::string::npos);
}

} // namespace
} // namespace engine::render::opengl
// NOLINTEND
