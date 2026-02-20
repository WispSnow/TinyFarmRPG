// NOLINTBEGIN
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

namespace engine::core {
namespace {

[[nodiscard]] std::string readTextFile(const std::filesystem::path& path) {
    std::ifstream file(path);
    EXPECT_TRUE(file.is_open()) << path;
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

TEST(GameAppDispatcherTraceTest, RunLoopMarksQueueDispatchForUpdate) {
    const std::filesystem::path source_path =
        (std::filesystem::path{PROJECT_SOURCE_DIR} / "src/engine/core/game_app.cpp").lexically_normal();
    ASSERT_TRUE(std::filesystem::exists(source_path)) << source_path;

    const std::string source = readTextFile(source_path);
    ASSERT_FALSE(source.empty());

    EXPECT_EQ(source.find("debug_ui_manager_->onFrameBegin()"), std::string::npos)
        << "Frame boundary hook should be removed now that DispatcherTrace tracks recent dispatches instead of per-frame counters.";
    EXPECT_NE(source.find("debug_ui_manager_->onDispatcherUpdateBegin()"), std::string::npos)
        << "GameApp should allow the debug UI to observe dispatcher.update() scope (begin).";
    EXPECT_NE(source.find("debug_ui_manager_->onDispatcherUpdateEnd()"), std::string::npos)
        << "GameApp should allow the debug UI to observe dispatcher.update() scope (end).";

    EXPECT_NE(source.find("time_->tryConsumeFixedTick()"), std::string::npos)
        << "GameApp run loop should use fixed-step consumption from Time.";
    EXPECT_NE(source.find("time_->getFixedDeltaTime()"), std::string::npos)
        << "GameApp run loop should drive update with fixed delta time.";
    EXPECT_NE(source.find("updateFrame(time_->getUnscaledDeltaTime())"), std::string::npos)
        << "GameApp run loop should execute per-frame update once after fixed ticks.";
    EXPECT_NE(source.find("const size_t frame_stack_size_before = scene_manager_->getSceneStackSize();"), std::string::npos)
        << "GameApp should snapshot scene stack before frame update for accumulator reset safety.";
    EXPECT_NE(source.find("frame_current_scene_before != frame_current_scene_after"), std::string::npos)
        << "GameApp should clear accumulator when frame update triggers scene switching.";
    EXPECT_NE(source.find("time_->getInterpolationAlpha()"), std::string::npos)
        << "GameApp run loop should sample interpolation alpha from Time.";
    EXPECT_NE(source.find("config_->render_interpolation_enabled_"), std::string::npos)
        << "GameApp run loop should gate interpolation by config.";
    EXPECT_NE(source.find("? time_->getInterpolationAlpha()"), std::string::npos)
        << "GameApp should use Time::getInterpolationAlpha when interpolation is enabled.";
    EXPECT_NE(source.find(": 1.0f;"), std::string::npos)
        << "GameApp should fall back to alpha=1.0f when interpolation is disabled.";
    EXPECT_NE(source.find("render(interpolation_alpha);"), std::string::npos)
        << "GameApp run loop should pass alpha into render stage.";
    EXPECT_NE(source.find("time_->clearAccumulator()"), std::string::npos)
        << "GameApp should clear accumulator when scene stack changes.";
    EXPECT_NE(source.find("scene_manager_->fixedUpdate(delta_time)"), std::string::npos)
        << "GameApp fixed-step update should route to SceneManager::fixedUpdate.";
    EXPECT_NE(source.find("input_manager_->sampleInputEvents()"), std::string::npos)
        << "Input events should be sampled once per render frame.";
    EXPECT_NE(source.find("input_manager_->dispatchActionCallbacks()"), std::string::npos)
        << "Input callbacks should be dispatched per fixed tick.";
    EXPECT_NE(source.find("input_manager_->consumeTick()"), std::string::npos)
        << "Input transient states should be consumed per fixed tick.";
    EXPECT_NE(source.find("time_->setFixedDeltaTime(1.0F / static_cast<float>(config_->logic_tick_hz_));"), std::string::npos)
        << "GameApp initTime should apply fixed tick frequency from config.";
    EXPECT_NE(source.find("time_->setMaxTicksPerFrame(config_->max_ticks_per_frame_);"), std::string::npos)
        << "GameApp initTime should apply catch-up cap from config.";

    const auto render_pos = source.find("render(interpolation_alpha);");
    const auto dispatch_pos = source.find("dispatcher_->update();");
    ASSERT_NE(render_pos, std::string::npos);
    ASSERT_NE(dispatch_pos, std::string::npos);
    EXPECT_LT(render_pos, dispatch_pos)
        << "Queued event dispatch should remain after render to preserve frame-tail semantics.";
}

} // namespace
} // namespace engine::core
// NOLINTEND
