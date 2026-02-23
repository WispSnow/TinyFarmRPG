// NOLINTBEGIN
#include <gtest/gtest.h>

#include "game/runtime/system_scheduler.h"
#include "game/runtime/system_bundle.h"

#include <entt/entity/registry.hpp>
#include <entt/signal/dispatcher.hpp>

#include <algorithm>
#include <vector>

namespace game::runtime {
namespace {

[[nodiscard]] std::vector<SchedulerStage> traceStages(const SystemScheduler::TickResult& result) {
    std::vector<SchedulerStage> stages;
    stages.reserve(result.trace.stages.size());
    for (const auto& sample : result.trace.stages) {
        stages.push_back(sample.stage);
    }
    return stages;
}

[[nodiscard]] bool contains(const std::vector<SchedulerStage>& stages, const SchedulerStage target) {
    return std::find(stages.begin(), stages.end(), target) != stages.end();
}

TEST(SystemSchedulerTransitionGateTest, Gate1RunsOnlyTransitionBranchWhenAlreadyActive) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    GameSystemBundle systems;
    SystemScheduler scheduler;

    const auto result = scheduler.tick({
        .mode = GameMode::Exploration,
        .systems = systems,
        .registry = registry,
        .dispatcher = &dispatcher,
        .delta_time = 0.016f,
        .is_transition_active = []() { return true; }
    });

    const auto stages = traceStages(result);
    ASSERT_EQ(stages.size(), 3U);
    EXPECT_EQ(stages[0], SchedulerStage::RemoveEntity);
    EXPECT_EQ(stages[1], SchedulerStage::TransitionUpdatePre);
    EXPECT_EQ(stages[2], SchedulerStage::LightTogglePre);
    EXPECT_TRUE(result.gate1_triggered);
    EXPECT_FALSE(result.gate2_triggered);
    EXPECT_FALSE(contains(stages, SchedulerStage::Time));
    EXPECT_FALSE(contains(stages, SchedulerStage::SpatialIndex));
}

TEST(SystemSchedulerTransitionGateTest, Gate2SkipsPostGameplayWhenTransitionActivatesLater) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    GameSystemBundle systems;
    SystemScheduler scheduler;
    int transition_active_calls = 0;

    const auto result = scheduler.tick({
        .mode = GameMode::Exploration,
        .systems = systems,
        .registry = registry,
        .dispatcher = &dispatcher,
        .delta_time = 0.016f,
        .is_transition_active = [&]() {
            ++transition_active_calls;
            return transition_active_calls >= 2;
        }
    });

    const auto stages = traceStages(result);
    EXPECT_TRUE(contains(stages, SchedulerStage::Movement));
    EXPECT_TRUE(contains(stages, SchedulerStage::TransitionUpdatePost));
    EXPECT_TRUE(contains(stages, SchedulerStage::LightTogglePost));
    EXPECT_FALSE(result.gate1_triggered);
    EXPECT_TRUE(result.gate2_triggered);
    EXPECT_FALSE(contains(stages, SchedulerStage::SpatialIndex));
    EXPECT_FALSE(contains(stages, SchedulerStage::Animation));
}

TEST(SystemSchedulerTransitionGateTest, NormalPathReachesPostGameplayStages) {
    entt::registry registry;
    entt::dispatcher dispatcher;
    GameSystemBundle systems;
    SystemScheduler scheduler;

    const auto result = scheduler.tick({
        .mode = GameMode::Exploration,
        .systems = systems,
        .registry = registry,
        .dispatcher = &dispatcher,
        .delta_time = 0.016f,
        .is_transition_active = []() { return false; }
    });

    const auto stages = traceStages(result);
    EXPECT_FALSE(result.gate1_triggered);
    EXPECT_FALSE(result.gate2_triggered);
    EXPECT_TRUE(contains(stages, SchedulerStage::SpatialIndex));
    EXPECT_TRUE(contains(stages, SchedulerStage::Pickup));
    EXPECT_TRUE(contains(stages, SchedulerStage::Interaction));
    EXPECT_TRUE(contains(stages, SchedulerStage::CameraFollow));
    EXPECT_TRUE(contains(stages, SchedulerStage::Animation));
}

} // namespace
} // namespace game::runtime
// NOLINTEND
