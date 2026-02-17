// NOLINTBEGIN
#include <gtest/gtest.h>

#include "game/runtime/game_mode.h"
#include "game/runtime/system_scheduler.h"
#include "game/runtime/system_bundle.h"

#include <entt/entity/registry.hpp>

#include <algorithm>
#include <vector>

namespace game::runtime {
namespace {

[[nodiscard]] size_t indexOf(const std::vector<SchedulerStage>& stages, SchedulerStage target) {
    const auto it = std::find(stages.begin(), stages.end(), target);
    EXPECT_NE(it, stages.end()) << "missing stage: " << toString(target);
    if (it == stages.end()) {
        return stages.size();
    }
    return static_cast<size_t>(std::distance(stages.begin(), it));
}

TEST(SystemSchedulerProfileTest, ExplorationKeepsMovementBeforeSpatialIndex) {
    const auto& stages = SystemScheduler::profileStages(GameMode::Exploration);
    ASSERT_FALSE(stages.empty());

    const size_t movement = indexOf(stages, SchedulerStage::Movement);
    const size_t spatial_index = indexOf(stages, SchedulerStage::SpatialIndex);
    EXPECT_LT(movement, spatial_index);
}

TEST(SystemSchedulerProfileTest, ExplorationKeepsTimeBeforeDayNight) {
    const auto& stages = SystemScheduler::profileStages(GameMode::Exploration);
    ASSERT_FALSE(stages.empty());

    const size_t time = indexOf(stages, SchedulerStage::Time);
    const size_t day_night = indexOf(stages, SchedulerStage::DayNight);
    EXPECT_LT(time, day_night);
}

TEST(SystemSchedulerProfileTest, ExplorationTickMatchesProfileOrderWhenNoTransition) {
    entt::registry registry;
    GameSystemBundle systems;
    SystemScheduler scheduler;
    std::vector<SchedulerStage> executed;

    const auto result = scheduler.tick({
        GameMode::Exploration,
        systems,
        registry,
        0.016f,
        [&](SchedulerStage stage) { executed.push_back(stage); },
        []() { return false; }
    });

    const auto& profile = SystemScheduler::profileStages(GameMode::Exploration);
    std::vector<SchedulerStage> expected;
    expected.reserve(profile.size());
    for (const auto stage : profile) {
        if (stage == SchedulerStage::TransitionUpdatePre || stage == SchedulerStage::LightTogglePre) {
            continue;
        }
        expected.push_back(stage);
    }

    EXPECT_FALSE(result.gate1_triggered);
    EXPECT_FALSE(result.gate2_triggered);
    EXPECT_EQ(executed, expected);
}

} // namespace
} // namespace game::runtime
// NOLINTEND
