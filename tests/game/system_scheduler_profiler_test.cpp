// NOLINTBEGIN
#include <gtest/gtest.h>

#include "game/debug/scheduler_profiler.h"
#include "game/runtime/system_bundle.h"

#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>

#include <cstddef>

namespace game::debug {
namespace {

[[nodiscard]] game::runtime::SystemScheduler::TickResult makeSingleStageTickResult(
    const game::runtime::SchedulerStage stage,
    const double elapsed_ms,
    const bool gate1 = false,
    const bool gate2 = false) {
    game::runtime::SystemScheduler::TickResult result{};
    result.gate1_triggered = gate1;
    result.gate2_triggered = gate2;
    result.trace.stages.push_back(game::runtime::SystemScheduler::StageTrace{
        stage,
        elapsed_ms
    });
    return result;
}

TEST(SystemSchedulerProfilerTest, CaptureFrameCollectsSchedulerTrace) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    game::runtime::GameSystemBundle systems;
    game::runtime::SystemScheduler scheduler;
    SchedulerProfiler profiler;
    profiler.setEnabled(true);

    const auto tick_result = scheduler.tick({
        .mode = game::runtime::GameMode::Exploration,
        .systems = systems,
        .registry = registry,
        .dispatcher = &dispatcher,
        .delta_time = 0.016f,
        .is_transition_active = []() { return false; }
    });
    profiler.captureFrame(game::runtime::GameMode::Exploration, tick_result, false);

    const auto* frame = profiler.latestFrame();
    ASSERT_NE(frame, nullptr);
    EXPECT_EQ(frame->mode, game::runtime::GameMode::Exploration);
    EXPECT_FALSE(frame->gate1_triggered);
    EXPECT_FALSE(frame->gate2_triggered);
    ASSERT_EQ(frame->stages.size(), tick_result.trace.stages.size());

    double total_elapsed = 0.0;
    for (std::size_t i = 0; i < frame->stages.size(); ++i) {
        EXPECT_EQ(frame->stages[i].stage, tick_result.trace.stages[i].stage);
        EXPECT_EQ(frame->stages[i].elapsed_ms, tick_result.trace.stages[i].elapsed_ms);
        EXPECT_GE(frame->stages[i].elapsed_ms, 0.0);
        total_elapsed += frame->stages[i].elapsed_ms;
    }
    EXPECT_DOUBLE_EQ(frame->total_ms, total_elapsed);
}

TEST(SystemSchedulerProfilerTest, CaptureFrameRecordsProvidedTraceAndGateFlags) {
    SchedulerProfiler profiler;
    profiler.setEnabled(true);

    game::runtime::SystemScheduler::TickResult tick_result{};
    tick_result.gate1_triggered = false;
    tick_result.gate2_triggered = true;
    tick_result.trace.stages.push_back(game::runtime::SystemScheduler::StageTrace{
        game::runtime::SchedulerStage::RemoveEntity,
        0.125
    });
    tick_result.trace.stages.push_back(game::runtime::SystemScheduler::StageTrace{
        game::runtime::SchedulerStage::TransitionUpdatePost,
        0.250
    });

    profiler.captureFrame(game::runtime::GameMode::Cutscene, tick_result, false);

    const auto* frame = profiler.latestFrame();
    ASSERT_NE(frame, nullptr);
    EXPECT_EQ(frame->mode, game::runtime::GameMode::Cutscene);
    EXPECT_FALSE(frame->gate1_triggered);
    EXPECT_TRUE(frame->gate2_triggered);
    ASSERT_EQ(frame->stages.size(), 2U);
    EXPECT_EQ(frame->stages[0].stage, game::runtime::SchedulerStage::RemoveEntity);
    EXPECT_EQ(frame->stages[1].stage, game::runtime::SchedulerStage::TransitionUpdatePost);
    EXPECT_DOUBLE_EQ(frame->stages[0].elapsed_ms, 0.125);
    EXPECT_DOUBLE_EQ(frame->stages[1].elapsed_ms, 0.250);
    EXPECT_DOUBLE_EQ(frame->total_ms, 0.375);
}

TEST(SystemSchedulerProfilerTest, CaptureOffKeepsHistoryEmpty) {
    SchedulerProfiler profiler;
    profiler.setEnabled(false);

    profiler.captureFrame(
        game::runtime::GameMode::Exploration,
        makeSingleStageTickResult(game::runtime::SchedulerStage::RemoveEntity, 0.1),
        false);

    EXPECT_EQ(profiler.frameCount(), 0U);
    EXPECT_EQ(profiler.latestFrame(), nullptr);
}

TEST(SystemSchedulerProfilerTest, FixedCapacityKeepsRecentFrames) {
    SchedulerProfiler profiler(/*max_frames=*/2);
    profiler.setEnabled(true);

    const auto push_frame = [&](const game::runtime::GameMode mode,
                                const game::runtime::SchedulerStage stage,
                                const double elapsed_ms) {
        profiler.captureFrame(mode, makeSingleStageTickResult(stage, elapsed_ms), false);
    };

    push_frame(game::runtime::GameMode::Exploration, game::runtime::SchedulerStage::RemoveEntity, 0.1);
    push_frame(game::runtime::GameMode::Battle, game::runtime::SchedulerStage::RemoveEntity, 0.2);
    push_frame(game::runtime::GameMode::Cutscene, game::runtime::SchedulerStage::Time, 0.3);

    EXPECT_EQ(profiler.frameCount(), 2U);

    const auto recent = profiler.recentFrames(/*max_count=*/10);
    ASSERT_EQ(recent.size(), 2U);
    EXPECT_EQ(recent[0].mode, game::runtime::GameMode::Cutscene);
    EXPECT_EQ(recent[1].mode, game::runtime::GameMode::Battle);
}

} // namespace
} // namespace game::debug
// NOLINTEND
