// NOLINTBEGIN
#include <gtest/gtest.h>

#include "game/debug/scheduler_profiler.h"
#include "game/runtime/system_bundle.h"

#include <entt/entity/registry.hpp>

#include <vector>

namespace game::debug {
namespace {

TEST(SystemSchedulerProfilerTest, StageHooksCanCollectDurations) {
    entt::registry registry;
    game::runtime::GameSystemBundle systems;
    game::runtime::SystemScheduler scheduler;
    SchedulerProfiler profiler;
    profiler.setEnabled(true);

    std::vector<game::runtime::SchedulerStage> started;
    std::vector<game::runtime::SchedulerStage> completed;

    profiler.beginFrame(game::runtime::GameMode::Exploration);
    const auto result = scheduler.tick({
        game::runtime::GameMode::Exploration,
        systems,
        registry,
        0.016f,
        {},
        []() { return false; },
        [&](const game::runtime::SchedulerStage stage) {
            started.push_back(stage);
            profiler.onStageStarted(stage);
        },
        [&](const game::runtime::SchedulerStage stage) {
            completed.push_back(stage);
            profiler.onStageCompleted(stage);
        }
    });
    profiler.endFrame(result, false);

    ASSERT_EQ(started, completed);

    const auto* frame = profiler.latestFrame();
    ASSERT_NE(frame, nullptr);
    EXPECT_EQ(frame->mode, game::runtime::GameMode::Exploration);
    EXPECT_FALSE(frame->gate1_triggered);
    EXPECT_FALSE(frame->gate2_triggered);
    ASSERT_EQ(frame->stages.size(), started.size());

    for (const auto& stage : frame->stages) {
        EXPECT_GE(stage.elapsed_ms, 0.0);
    }
}

TEST(SystemSchedulerProfilerTest, CaptureOnRecordsStageOrderAndDurations) {
    SchedulerProfiler profiler;
    profiler.setEnabled(true);

    const game::runtime::SystemScheduler::TickResult tick_result{
        false,
        true
    };

    profiler.beginFrame(game::runtime::GameMode::Cutscene);
    profiler.onStageStarted(game::runtime::SchedulerStage::RemoveEntity);
    profiler.onStageCompleted(game::runtime::SchedulerStage::RemoveEntity);
    profiler.onStageStarted(game::runtime::SchedulerStage::TransitionUpdatePost);
    profiler.onStageCompleted(game::runtime::SchedulerStage::TransitionUpdatePost);
    profiler.endFrame(tick_result, false);

    const auto* frame = profiler.latestFrame();
    ASSERT_NE(frame, nullptr);
    EXPECT_EQ(frame->mode, game::runtime::GameMode::Cutscene);
    EXPECT_FALSE(frame->gate1_triggered);
    EXPECT_TRUE(frame->gate2_triggered);
    ASSERT_EQ(frame->stages.size(), 2U);
    EXPECT_EQ(frame->stages[0].stage, game::runtime::SchedulerStage::RemoveEntity);
    EXPECT_EQ(frame->stages[1].stage, game::runtime::SchedulerStage::TransitionUpdatePost);
    EXPECT_GE(frame->stages[0].elapsed_ms, 0.0);
    EXPECT_GE(frame->stages[1].elapsed_ms, 0.0);
}

TEST(SystemSchedulerProfilerTest, CaptureOffKeepsHistoryEmpty) {
    SchedulerProfiler profiler;
    profiler.setEnabled(false);

    profiler.beginFrame(game::runtime::GameMode::Exploration);
    profiler.onStageStarted(game::runtime::SchedulerStage::RemoveEntity);
    profiler.onStageCompleted(game::runtime::SchedulerStage::RemoveEntity);
    profiler.endFrame(game::runtime::SystemScheduler::TickResult{}, false);

    EXPECT_EQ(profiler.frameCount(), 0U);
    EXPECT_EQ(profiler.latestFrame(), nullptr);
}

TEST(SystemSchedulerProfilerTest, FixedCapacityKeepsRecentFrames) {
    SchedulerProfiler profiler(/*max_frames=*/2);
    profiler.setEnabled(true);

    const auto push_frame = [&](const game::runtime::GameMode mode,
                                const game::runtime::SchedulerStage stage) {
        profiler.beginFrame(mode);
        profiler.onStageStarted(stage);
        profiler.onStageCompleted(stage);
        profiler.endFrame(game::runtime::SystemScheduler::TickResult{}, false);
    };

    push_frame(game::runtime::GameMode::Exploration, game::runtime::SchedulerStage::RemoveEntity);
    push_frame(game::runtime::GameMode::Battle, game::runtime::SchedulerStage::RemoveEntity);
    push_frame(game::runtime::GameMode::Cutscene, game::runtime::SchedulerStage::Time);

    EXPECT_EQ(profiler.frameCount(), 2U);

    const auto recent = profiler.recentFrames(/*max_count=*/10);
    ASSERT_EQ(recent.size(), 2U);
    EXPECT_EQ(recent[0].mode, game::runtime::GameMode::Cutscene);
    EXPECT_EQ(recent[1].mode, game::runtime::GameMode::Battle);
}

} // namespace
} // namespace game::debug
// NOLINTEND
