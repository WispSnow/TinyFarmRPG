// NOLINTBEGIN
#include <gtest/gtest.h>

#include <filesystem>
#include <string>

#include "test_source_utils.h"

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

namespace engine::ui::rmlui {
namespace {

TEST(RmlUiDebuggerPipelineTest, LayerOwnsDebuggerLifecycle) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/ui/rmlui/rml_ui_layer.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string content = test_source_utils::readTextFile(source_path);
    ASSERT_FALSE(content.empty()) << "无法读取: " << source_path;

    const std::string enable_block =
        test_source_utils::extractFunctionBlock(content, "void RmlUILayer::setDebuggerEnabled(");
    ASSERT_FALSE(enable_block.empty());
    EXPECT_NE(enable_block.find("Rml::Debugger::Shutdown();"), std::string::npos);

    const std::string init_block =
        test_source_utils::extractFunctionBlock(content, "bool RmlUILayer::ensureDebuggerInitialized()");
    ASSERT_FALSE(init_block.empty());
    EXPECT_NE(init_block.find("Rml::Debugger::Initialise(context_)"), std::string::npos);

    const std::string visible_block =
        test_source_utils::extractFunctionBlock(content, "void RmlUILayer::setDebuggerVisible(");
    ASSERT_FALSE(visible_block.empty());
    EXPECT_NE(visible_block.find("Rml::Debugger::SetVisible(visible);"), std::string::npos);
}

} // namespace
} // namespace engine::ui::rmlui

namespace engine::render::opengl {
namespace {

TEST(RmlUiDebuggerPipelineTest, GlRendererHandlesF4AndForwardsRuntimeToggle) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/render/opengl/gl_renderer.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string content = test_source_utils::readTextFile(source_path);
    ASSERT_FALSE(content.empty()) << "无法读取: " << source_path;

    const std::string handle_event_block =
        test_source_utils::extractFunctionBlock(content, "void GLRenderer::handleSDLEvent(const SDL_Event& event)");
    ASSERT_FALSE(handle_event_block.empty());
    EXPECT_NE(handle_event_block.find("event.key.scancode == SDL_SCANCODE_F4"), std::string::npos);
    EXPECT_NE(handle_event_block.find("toggleRmlUiDebuggerVisible();"), std::string::npos);

    const std::string init_rmlui_block =
        test_source_utils::extractFunctionBlock(content, "bool GLRenderer::initRmlUiLayer()");
    ASSERT_FALSE(init_rmlui_block.empty());
    EXPECT_NE(init_rmlui_block.find("rmlui_layer_->setDebuggerEnabled(rmlui_debugger_enabled_);"), std::string::npos);

    const std::string set_debugger_block =
        test_source_utils::extractFunctionBlock(content, "void GLRenderer::setRmlUiDebuggerEnabled(bool enabled)");
    ASSERT_FALSE(set_debugger_block.empty());
    EXPECT_NE(set_debugger_block.find("rmlui_debugger_enabled_ = enabled;"), std::string::npos);
    EXPECT_NE(set_debugger_block.find("rmlui_layer_->setDebuggerEnabled(enabled);"), std::string::npos);
}

} // namespace
} // namespace engine::render::opengl

namespace engine::core {
namespace {

TEST(RmlUiDebuggerPipelineTest, GameAppAppliesConfiguredDebuggerToggleAndRegistersObserver) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/core/game_app.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string content = test_source_utils::readTextFile(source_path);
    ASSERT_FALSE(content.empty()) << "无法读取: " << source_path;

    const std::string init_gl_renderer_block =
        test_source_utils::extractFunctionBlock(content, "bool GameApp::initGLRenderer()");
    ASSERT_FALSE(init_gl_renderer_block.empty());
    EXPECT_NE(init_gl_renderer_block.find("gl_renderer_->setRmlUiDebuggerEnabled(config_->rmlui_debugger_enabled_);"),
              std::string::npos);

    const std::string init_input_block =
        test_source_utils::extractFunctionBlock(content, "bool GameApp::initInputManager()");
    ASSERT_FALSE(init_input_block.empty());
    EXPECT_NE(init_input_block.find("input_manager_->setSdlEventObserver("), std::string::npos);
    EXPECT_NE(init_input_block.find("gl_renderer_->handleSDLEvent(event);"), std::string::npos);
}

} // namespace
} // namespace engine::core
// NOLINTEND
